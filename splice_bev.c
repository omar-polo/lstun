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

#if !HAVE_SO_SPLICE

#include <sys/socket.h>

#include "log.h"
#include "lstun.h"

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
}

int
conn_splice(struct conn *c)
{
	c->sourcebev = bufferevent_new(c->source, sreadcb, nopcb, errcb, c);
	c->tobev = bufferevent_new(c->to, treadcb, nopcb, errcb, c);

	if (c->sourcebev == NULL || c->tobev == NULL) {
		log_warn("bufferevent_new");
		return -1;
	}

	bufferevent_enable(c->sourcebev, EV_READ|EV_WRITE);
	bufferevent_enable(c->tobev, EV_READ|EV_WRITE);
	return 0;
}

#endif	/* !HAVE_SO_SPLICE */
