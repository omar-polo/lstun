/*
 * Copyright (c) 2021, 2022 Omar Polo <op@omarpolo.com>
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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "log.h"

#ifndef SSH_PATH
#define SSH_PATH "/usr/bin/ssh"
#endif

#define MAXSOCK 32
#define BACKOFF 1
#define RETRIES 16

#ifndef __OpenBSD__
#define pledge(p, e) 0
#define unveil(p, m) 0
#endif

const char	*addr;		/* our addr */
const char	*ssh_tflag;
const char	*ssh_dest;

char		 ssh_host[256];
char		 ssh_port[16];

struct event	 sockev[MAXSOCK];
int		 socks[MAXSOCK];
int		 nsock;

int		 debug;
int		 verbose;

struct event	 sighupev;
struct event	 sigintev;
struct event	 sigtermev;
struct event	 sigchldev;
struct event	 siginfoev;

struct timeval	 timeout = {600, 0}; /* 10 minutes */
struct event	 timeoutev;

pid_t		 ssh_pid = -1;

int		 conn;

struct conn {
	int			 ntentative;
	struct timeval		 retry;
	struct event		 waitev;
	int			 source;
	struct bufferevent	*sourcebev;
	int			 to;
	struct bufferevent	*tobev;
};

static void
sig_handler(int sig, short event, void *data)
{
	int status;

	switch (sig) {
	case SIGHUP:
	case SIGINT:
	case SIGTERM:
		log_info("quitting");
		event_loopbreak();
		break;
	case SIGCHLD:
		if (waitpid(ssh_pid, &status, WNOHANG) == -1)
			fatal("waitpid");
		ssh_pid = -1;
		break;
#ifdef SIGINFO
	case SIGINFO:
#else
	case SIGUSR1:
#endif
		log_info("connections: %d", conn);
	}
}

static int
spawn_ssh(void)
{
	log_debug("spawning ssh");

	switch (ssh_pid = fork()) {
	case -1:
		log_warnx("fork");
		return -1;
	case 0:
		execl(SSH_PATH, "ssh", "-L", ssh_tflag, "-NTq", ssh_dest,
		    NULL);
		fatal("exec");
	default:
		return 0;
	}
}

static void
conn_free(struct conn *c)
{
	if (c->sourcebev != NULL)
		bufferevent_free(c->sourcebev);
	if (c->tobev != NULL)
		bufferevent_free(c->tobev);

	if (evtimer_pending(&c->waitev, NULL))
		evtimer_del(&c->waitev);

	close(c->source);
	if (c->to != -1)
		close(c->to);

	free(c);
}

static void
killing_time(int fd, short event, void *data)
{
	if (ssh_pid == -1)
		return;

	log_debug("timeout expired, killing ssh (%d)", ssh_pid);
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

	log_info("closing connection (event=%x)", event);

	conn_free(c);

	if (--conn == 0) {
		log_debug("scheduling ssh termination (%llds)",
		    (long long)timeout.tv_sec);
		if (timeout.tv_sec != 0) {
			evtimer_set(&timeoutev, killing_time, NULL);
			evtimer_add(&timeoutev, &timeout);
		}
	}
}

static int
connect_to_ssh(void)
{
	struct addrinfo hints, *res, *res0;
	int r, saved_errno, sock;
	const char *cause;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	r = getaddrinfo(ssh_host, ssh_port, &hints, &res0);
	if (r != 0) {
		log_warnx("getaddrinfo(\"%s\", \"%s\"): %s",
		    ssh_host, ssh_port, gai_strerror(r));
		return -1;
	}

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
		log_warn("%s", cause);

	freeaddrinfo(res0);
	return sock;
}

static void
try_to_connect(int fd, short event, void *d)
{
	struct conn *c = d;

	/* ssh may have died in the meantime */
	if (ssh_pid == -1) {
		conn_free(c);
		return;
	}

	c->ntentative++;
	log_info("trying to connect to %s:%s (%d/%d)", ssh_host, ssh_port,
	    c->ntentative, RETRIES);

	if ((c->to = connect_to_ssh()) == -1) {
		if (c->ntentative == RETRIES) {
			log_warnx("giving up connecting");
			conn_free(c);
			return;
		}

		evtimer_set(&c->waitev, try_to_connect, c);
		evtimer_add(&c->waitev, &c->retry);
		return;
	}

	log_info("connected!");

	c->sourcebev = bufferevent_new(c->source, sreadcb, nopcb, errcb, c);
	c->tobev = bufferevent_new(c->to, treadcb, nopcb, errcb, c);
	if (c->sourcebev == NULL || c->tobev == NULL) {
		log_warn("bufferevent_new");
		conn_free(c);
		return;
	}

	bufferevent_enable(c->sourcebev, EV_READ|EV_WRITE);
	bufferevent_enable(c->tobev, EV_READ|EV_WRITE);
}

static void
do_accept(int fd, short event, void *data)
{
	struct conn *c;
	int s;

	log_debug("incoming connection");

	if ((s = accept(fd, NULL, 0)) == -1) {
		log_warn("accept");
		return;
	}

	if (ssh_pid == -1 && spawn_ssh() == -1) {
		close(s);
		return;
	}

	if ((c = calloc(1, sizeof(*c))) == NULL) {
		log_warn("calloc");
		close(s);
		return;
	}

	conn++;
	if (evtimer_pending(&timeoutev, NULL))
		evtimer_del(&timeoutev);

	c->source = s;
	c->to = -1;
	c->retry.tv_sec = BACKOFF;
	evtimer_set(&c->waitev, try_to_connect, c);
	evtimer_add(&c->waitev, &c->retry);
}

static const char *
copysec(const char *s, char *d, size_t len)
{
	const char *c;

	if ((c = strchr(s, ':')) == NULL)
		return NULL;
	if ((size_t)(c - s) >= len-1)
		return NULL;
	memset(d, 0, len);
	memcpy(d, s, c - s);
	return c;
}

static void
bind_socket(void)
{
	struct addrinfo hints, *res, *res0;
	int v, r, saved_errno;
	char host[64];
	const char *c, *h, *port, *cause;

	if ((c = strchr(addr, ':')) == NULL) {
		h = NULL;
		port = addr;
	} else {
		if ((c = copysec(addr, host, sizeof(host))) == NULL)
			fatalx("name too long: %s", addr);

		h = host;
		port = c+1;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	r = getaddrinfo(h, port, &hints, &res0);
	if (r != 0)
		fatalx("getaddrinfo(%s): %s", addr, gai_strerror(r));

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

		v = 1;
		if (setsockopt(socks[nsock], SOL_SOCKET, SO_REUSEADDR, &v,
		    sizeof(v)) == -1)
			fatal("setsockopt(SO_REUSEADDR)");

		v = 1;
		if (setsockopt(socks[nsock], SOL_SOCKET, SO_REUSEPORT, &v,
		    sizeof(v)) == -1)
			fatal("setsockopt(SO_REUSEPORT)");

		listen(socks[nsock], 5);

		nsock++;
	}
	if (nsock == 0)
		fatal("%s", cause);

	freeaddrinfo(res0);
}

static void
parse_sshaddr(void)
{
	const char *c;

	if (isdigit((unsigned char)*ssh_tflag)) {
		strlcpy(ssh_host, "localhost", sizeof(ssh_host));
		if (copysec(ssh_tflag, ssh_port, sizeof(ssh_port)) == NULL)
			goto err;
		return;
	}

	if ((c = copysec(ssh_tflag, ssh_host, sizeof(ssh_host))) == NULL)
		goto err;
	if (copysec(c+1, ssh_port, sizeof(ssh_port)) == NULL)
		goto err;
	return;

err:
	fatalx("wrong value for -B");
}

static void __dead
usage(void)
{
	fprintf(stderr, "usage: %s [-dv] -B sshaddr -b addr [-t timeout]"
	    " destination\n", getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch, i, fd;
	const char *errstr;
	struct stat sb;

	/*
	 * Ensure we have fds 0-2 open so that we have no issue with
	 * calling bind_socket before daemon(3).
	 */
	for (i = 0; i < 3; ++i) {
		if (fstat(i, &sb) == -1) {
			if ((fd = open("/dev/null", O_RDWR)) != -1) {
				if (dup2(fd, i) == -1)
					exit(1);
				if (fd > i)
					close(fd);
			} else
				exit(1);
		}
	}

	log_init(1, LOG_DAEMON);
	log_setverbose(1);

	while ((ch = getopt(argc, argv, "B:b:dt:v")) != -1) {
		switch (ch) {
		case 'B':
			ssh_tflag = optarg;
			parse_sshaddr();
			break;
		case 'b':
			addr = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		case 't':
			timeout.tv_sec = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				fatalx("timeout is %s: %s", errstr, optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1 || addr == NULL || ssh_tflag == NULL)
		usage();

	ssh_dest = argv[0];

	bind_socket();

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if (!debug)
		daemon(1, 0);

	signal(SIGPIPE, SIG_IGN);

	event_init();

	/* initialize the timer */
	evtimer_set(&timeoutev, killing_time, NULL);

	signal_set(&sighupev, SIGHUP, sig_handler, NULL);
	signal_set(&sigintev, SIGINT, sig_handler, NULL);
	signal_set(&sigtermev, SIGTERM, sig_handler, NULL);
	signal_set(&sigchldev, SIGCHLD, sig_handler, NULL);
#ifdef SIGINFO
	signal_set(&siginfoev, SIGINFO, sig_handler, NULL);
#else
	signal_set(&siginfoev, SIGUSR1, sig_handler, NULL);
#endif

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

	if (unveil(SSH_PATH, "x") == -1)
		fatal("unveil(%s)", SSH_PATH);

	/*
	 * dns, inet: bind the socket and connect to the childs.
	 * proc, exec: execute ssh on demand.
	 */
	if (pledge("stdio dns inet proc exec", NULL) == -1)
		fatal("pledge");

	log_info("starting");
	event_dispatch();

	if (ssh_pid != -1)
		kill(ssh_pid, SIGINT);

	return 0;
}
