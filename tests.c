#if TEST_GETEXECNAME
#include <stdlib.h>

int
main(void)
{
	const char * progname;

	progname = getexecname();
	return progname == NULL;
}
#endif /* TEST_GETEXECNAME */
#if TEST_GETPROGNAME
#include <stdlib.h>

int
main(void)
{
	const char * progname;

	progname = getprogname();
	return progname == NULL;
}
#endif /* TEST_GETPROGNAME */
#if TEST_LIBEVENT
#include <event.h>

int
main(void)
{
	struct event ev;

	event_set(&ev, 0, EV_READ, NULL, NULL);
	event_add(&ev, NULL);
	event_del(&ev);
	return 0;
}
#endif /* TEST_LIBEVENT */
#if TEST_LIBEVENT2
#include <event2/event.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>
#include <event2/buffer_compat.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/bufferevent_compat.h>

int
main(void)
{
	struct event ev;

	event_set(&ev, 0, EV_READ, NULL, NULL);
	event_add(&ev, NULL);
	event_del(&ev);
	return 0;
}
#endif /* TEST_LIBEVENT2 */
#if TEST_LIB_SOCKET
#include <sys/socket.h>

int
main(void)
{
	int fds[2], c;

	c = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	return c == -1;
}
#endif /* TEST_LIB_SOCKET */
#if TEST_PLEDGE
#include <unistd.h>

int
main(void)
{
	return !!pledge("stdio", NULL);
}
#endif /* TEST_PLEDGE */
#if TEST_PROGRAM_INVOCATION_SHORT_NAME
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <errno.h>

int
main(void)
{

	return !program_invocation_short_name;
}
#endif /* TEST_PROGRAM_INVOCATION_SHORT_NAME */
#if TEST_PR_SET_NAME
#include <sys/prctl.h>

int
main(void)
{
	prctl(PR_SET_NAME, "foo");
	return 0;
}
#endif /* TEST_PR_SET_NAME */
#if TEST_STATIC
int
main(void)
{
	return 0; /* not meant to do anything */
}
#endif /* TEST_STATIC */
#if TEST_STRLCAT
#include <string.h>

int
main(void)
{
	char buf[3] = "a";
	return ! (strlcat(buf, "b", sizeof(buf)) == 2 &&
	    buf[0] == 'a' && buf[1] == 'b' && buf[2] == '\0');
}
#endif /* TEST_STRLCAT */
#if TEST_STRLCPY
#include <string.h>

int
main(void)
{
	char buf[2] = "";
	return ! (strlcpy(buf, "a", sizeof(buf)) == 1 &&
	    buf[0] == 'a' && buf[1] == '\0');
}
#endif /* TEST_STRLCPY */
#if TEST_STRTONUM
/*
 * Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#ifdef __NetBSD__
# define _OPENBSD_SOURCE
#endif
#include <stdlib.h>

int
main(void)
{
	const char *errstr;

	if (strtonum("1", 0, 2, &errstr) != 1)
		return 1;
	if (errstr != NULL)
		return 2;
	if (strtonum("1x", 0, 2, &errstr) != 0)
		return 3;
	if (errstr == NULL)
		return 4;
	if (strtonum("2", 0, 1, &errstr) != 0)
		return 5;
	if (errstr == NULL)
		return 6;
	if (strtonum("0", 1, 2, &errstr) != 0)
		return 7;
	if (errstr == NULL)
		return 8;
	return 0;
}
#endif /* TEST_STRTONUM */
#if TEST_UNVEIL
#include <unistd.h>

int
main(void)
{
	return -1 != unveil(NULL, NULL);
}
#endif /* TEST_UNVEIL */
#if TEST__MMD
int
main(void)
{
	return 0;
}
#endif /* TEST_NOOP */
#if TEST___PROGNAME
int
main(void)
{
	extern char *__progname;

	return !__progname;
}
#endif /* TEST___PROGNAME */
