/*
 * Copyright (c) 2022 Omar Polo <op@omarpolo.com>
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

#if HAVE_SO_SPLICE

#include <sys/socket.h>

#include "log.h"
#include "lstun.h"

static void
splice_done(int fd, short ev, void *data)
{
	struct conn *c = data;

	log_info("closing connection (event=%x)", ev);
	conn_free(c);
}

int
conn_splice(struct conn *c)
{
	if (setsockopt(c->source, SOL_SOCKET, SO_SPLICE, &c->to, sizeof(int))
	    == -1)
		return -1;
	if (setsockopt(c->to, SOL_SOCKET, SO_SPLICE, &c->source, sizeof(int))
	    == -1)
		return -1;

	event_once(c->source, EV_READ, splice_done, c, NULL);
	return 0;
}

#endif	/* HAVE_SO_SPLICE */
