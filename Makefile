PROG =		lstun
SRCS =		lstun.c log.c

LDADD =		-levent
DPADD =		${LIBEVENT}

WARNINGS =	yes

.include "lstun-version.mk"

.if "${LSTUN_RELEASE}" == "Yes"
PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/man/man
.else
NOMAN = Yes
PREFIX ?= ${HOME}
BINDIR ?= ${PREFIX}/bin
BINOWN ?= ${USER}
.if !defined(BINGRP)
BINGRP != id -g -n
.endif
DEBUG = -O0 -g
.endif

release:
	clean
	sed -i -e 's/_RELEASE=No/_RELEASE=Yes/' lstun-version.mk
	${MAKE} dist
	sed -i -e 's/_RELEASE=No/_RELEASE=No/' lstun-version.mk

dist: clean
	mkdir /tmp/lstun-${LSTUN_VERSION}
	pax -rw * /tmp/lstun-${LSTUN_VERSION}
	find /tmp/lstun-${LSTUN_VERSION} -type d -name obj -delete
	rm /tmp/lstun-${LSTUN_VERSION}/lstun-dist.txt
	tar -C /tmp -zcf lstun-${LSTUN_VERSION}.tar.gz lstun-${LSTUN_VERSION}
	rm -rf /tmp/lstun-${LSTUN_VERSION}/
	tar -ztf lstun-${LSTUN_VERSION}.tar.gz | \
		sed -e 's/^lstun-${LSTUN_VERSION}//' | \
		sort > lstun-dist.txt.new
	diff -u lstun-dist.txt lstun-dist.txt.new
	rm lstun-dist.txt.new

.include <bsd.prog.mk>
