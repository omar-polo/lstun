/*
 * Copyright (c) 2021 Omar Polo <op@omarpolo.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdint.h>

#ifndef SSH_PATH
#define SSH_PATH "/usr/bin/ssh"
#endif

#define MAXSOCK 4
#define MAXCONN 16

#ifndef __OpenBSD__
#define pledge(p, e) 0
#endif

int		 rport;		/* ssh port */
const char	*addr;		/* our addr */
const char	*ssh_tunnel_flag;
const char	*ssh_dest;

struct event	 sockev[MAXSOCK];
int		 socks[MAXSOCK];
int		 nsock;

struct event	 sighupev;
struct event	 sigintev;
struct event	 sigtermev;
struct event	 sigchldev;
struct event	 siginfoev;

struct timeval	 timeout;
struct event	 timeoutev;

pid_t		 ssh_pid = -1;

int		 conn;

struct conn {
	int			 source;
	struct bufferevent	*sourcebev;
	int			 to;
	struct bufferevent	*tobev;
} conns[MAXCONN];

static void
terminate(int fd, short event, void *data)
{
	event_loopbreak();
}

static void
chld(int fd, short event, void *data)
{
	int status;
	pid_t pid;

	if ((pid = waitpid(ssh_pid, &status, WNOHANG)) == -1)
		err(1, "waitpid");

	ssh_pid = -1;
}

static void
info(int fd, short event, void *data)
{
	warnx("connections: %d", conn);
}

static void
spawn_ssh(void)
{
	warnx("spawning ssh...");

	switch (ssh_pid = fork()) {
	case -1:
		err(1, "fork");
	case 0:
		execl(SSH_PATH, "ssh", "-L", ssh_tunnel_flag,
		    "-NTq", ssh_dest, NULL);
		err(1, "exec");
	default:
		/* TODO: wait just a bit to let ssh to do its things */
		sleep(5);
	}
}

static void
killing_time(int fd, short event, void *data)
{
	if (ssh_pid == -1)
		return;

	warnx("killing time!");
	kill(ssh_pid, SIGTERM);
	ssh_pid = -1;
}

static void
nopcb(struct bufferevent *bev, void *d)
{
	return;
}

static void
sreadcb(struct bufferevent *bev, void *d)
{
	struct conn *c = d;

	bufferevent_write_buffer(c->tobev, EVBUFFER_INPUT(bev));
}

static void
treadcb(struct bufferevent *bev, void *d)
{
	struct conn *c = d;

	bufferevent_write_buffer(c->sourcebev, EVBUFFER_INPUT(bev));
}

static void
errcb(struct bufferevent *bev, short event, void *d)
{
	struct conn *c = d;

	warnx("in errcb, closing connection");

	bufferevent_free(c->sourcebev);
	bufferevent_free(c->tobev);

	close(c->source);
	close(c->to);

	c->source = -1;
	c->to = -1;

	if (--conn == 0) {
		warnx("scheduling ssh termination (%llds)",
		    (long long)timeout.tv_sec);
		evtimer_set(&timeoutev, killing_time, NULL);
		evtimer_add(&timeoutev, &timeout);
	}
}

static int
connect_to_ssh(void)
{
	struct addrinfo hints, *res, *res0;
	int r, saved_errno, sock;
	char port[16];
	const char *c, *cause;

	if ((c = strchr(ssh_tunnel_flag, ':')) == NULL)
		errx(1, "wrong flag format: %s", ssh_tunnel_flag);

	if ((size_t)(c - ssh_tunnel_flag) >= sizeof(port)-1)
		errx(1, "EPORTTOOLONG");

	memset(port, 0, sizeof(port));
	memcpy(port, ssh_tunnel_flag, c - ssh_tunnel_flag);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* XXX: hardcoded */
	r = getaddrinfo("localhost", port, &hints, &res0);
	if (r != 0)
		errx(1, "getaddrinfo(\"localhost\", \"%s\"): %s",
		    port, gai_strerror(r));

	for (res = res0; res; res = res->ai_next) {
		sock = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (sock == -1) {
			cause = "socket";
			continue;
		}

		if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "connect";
			saved_errno = errno;
			close(sock);
			errno = saved_errno;
			sock = -1;
			continue;
		}

		break;
	}

	if (sock == -1)
		err(1, "%s", cause);

	freeaddrinfo(res0);
	return sock;
}

static void
do_accept(int fd, short event, void *data)
{
	int s, sock, i;

	warnx("handling connection");

	if (evtimer_pending(&timeoutev, NULL))
		evtimer_del(&timeoutev);

	if ((s = accept(fd, NULL, 0)) == -1)
		err(1, "accept");

	if (conn == MAXCONN) {
		/* oops */
		close(s);
		return;
	}

	conn++;

	if (ssh_pid == -1)
		spawn_ssh();

	warnx("binding the socket to ssh");
	sock = connect_to_ssh();

	for (i = 0; i < MAXCONN; ++i) {
		if (conns[i].source == -1) {
			conns[i].source = s;
			conns[i].to = sock;
			conns[i].sourcebev = bufferevent_new(s,
			    sreadcb, nopcb, errcb, &conns[i]);
			conns[i].tobev = bufferevent_new(sock,
			    treadcb, nopcb, errcb, &conns[i]);
			if (conns[i].sourcebev == NULL ||
			    conns[i].tobev == NULL)
				err(1, "bufferevent_new");
			bufferevent_enable(conns[i].sourcebev,
			    EV_READ|EV_WRITE);
			bufferevent_enable(conns[i].tobev,
			    EV_READ|EV_WRITE);
			break;
		}
	}
}

static void __dead
usage(void)
{
	fprintf(stderr, "usage: %s -B port:host:hostport -b addr [-t timeout]"
	    " destination\n", getprogname());
	exit(1);
}

static void
bind_socket(void)
{
	struct addrinfo hints, *res, *res0;
	int r, saved_errno;
	char host[64];
	const char *c, *h, *port, *cause;

	if ((c = strchr(addr, ':')) == NULL) {
		h = NULL;
		port = addr;
	} else {
		if ((size_t)(c - addr) >= sizeof(host) -1)
			errx(1, "ENAMETOOLONG");
		memset(host, 0, sizeof(host));
		memcpy(host, c, c - addr);

		h = host;
		port = c+1;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	r = getaddrinfo(h, port, &hints, &res0);
	if (r != 0)
		errx(1, "getaddrinfo(%s): %s",
		    addr, gai_strerror(r));

	for (res = res0; res && nsock < MAXSOCK; res = res->ai_next) {
		socks[nsock] = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (socks[nsock] == -1) {
			cause = "socket";
			continue;
		}

		if (bind(socks[nsock], res->ai_addr, res->ai_addrlen) == -1) {
			cause = "bind";
			saved_errno = errno;
			close(socks[nsock]);
			errno = saved_errno;
			continue;
		}

		listen(socks[nsock], 5);

		nsock++;
	}
	if (nsock == 0)
		err(1, "%s", cause);

	freeaddrinfo(res0);
}

int
main(int argc, char **argv)
{
	int ch, tout, i;
	const char *errstr;

	while ((ch = getopt(argc, argv, "B:b:t:")) != -1) {
		switch (ch) {
		case 'B':
			ssh_tunnel_flag = optarg;
			break;
		case 'b':
			addr = optarg;
			break;
		case 't':
			tout = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "timeout is %s: %s", errstr, optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1 || addr == NULL || ssh_tunnel_flag == NULL)
		usage();

	if (tout == 0)
		tout = 120;

	timeout.tv_sec = tout;
	timeout.tv_usec = 0;

	ssh_dest = argv[0];

	for (i = 0; i < MAXCONN; ++i) {
		conns[i].source = -1;
		conns[i].to = -1;
	}

	bind_socket();

	signal(SIGPIPE, SIG_IGN);

	event_init();

	/* initialize the timer */
	evtimer_set(&timeoutev, killing_time, NULL);

	signal_set(&sighupev, SIGHUP, terminate, NULL);
	signal_set(&sigintev, SIGINT, terminate, NULL);
	signal_set(&sigtermev, SIGTERM, terminate, NULL);
	signal_set(&sigchldev, SIGCHLD, chld, NULL);
	signal_set(&siginfoev, SIGINFO, info, NULL);

	signal_add(&sighupev, NULL);
	signal_add(&sigintev, NULL);
	signal_add(&sigtermev, NULL);
	signal_add(&sigchldev, NULL);
	signal_add(&siginfoev, NULL);

	for (i = 0; i < nsock; ++i) {
		event_set(&sockev[i], socks[i], EV_READ|EV_PERSIST,
		    do_accept, NULL);
		event_add(&sockev[i], NULL);
	}

	/*
	 * dns, inet: bind the socket and connect to the childs.
	 * proc, exec: execute ssh on demand.
	 */
	if (pledge("stdio dns inet proc exec", NULL) == -1)
		err(1, "pledge");

	warnx("lift off!");
	event_dispatch();

	if (ssh_pid != -1)
		kill(ssh_pid, SIGINT);

	return 0;
}
