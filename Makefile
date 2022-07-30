.PHONY: all clean distclean install

VERSION =	0.4
PROG =		lstun
DISTNAME =	${PROG}-${VERSION}

HEADERS =	log.h \
		lstun.h

SOURCES =	compats.c \
		log.c \
		lstun.c \
		splice.c \
		splice_bev.c \
		tests.c

OBJS =		${SOURCES:.c=.o}

DISTFILES =	CHANGES \
		LICENSE \
		Makefile \
		README.md \
		configure \
		lstun.1 \
		${HEADERS} \
		${SOURCES}

all: ${PROG}

Makefile.configure config.h: configure tests.c
	@echo "$@ is out of date; please run ./configure"
	@exit 1

include Makefile.configure

# -- targets --

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS} ${LDADD}

clean:
	rm -f ${OBJS} ${OBJS:.o=.d} ${PROG}

distclean: clean
	rm -f Makefile.configure config.h config.h.old config.log config.log.old

install: ${PROG}
	mkdir -p ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MANDIR}/man1
	${INSTALL_PROGRAM} ${PROG} ${DESTDIR}${BINDIR}
	${INSTALL_MAN} lstun.1 ${DESTDIR}${MANDIR}/man1/${PROG.1}

install-local: ${PROG}
	mkdir -p ${HOME}/bin
	${INSTALL_PROGRAM} ${PROG} ${HOME}/bin

uninstall:
	rm ${DESTDIR}${BINDIR}/${PROG}
	rm ${DESTDIR}${MANDIR}/man1/${PROG}.1

# --- maintainer targets ---

dist: ${DISTNAME}.sha256

${DISTNAME}.sha256: ${DISTNAME}.tar.gz
	sha256 ${DISTNAME}.tar.gz > $@

${DISTNAME}.tar.gz: ${DISTFILES}
	mkdir -p .dist/${DISTNAME}
	${INSTALL} -m 0644 ${DISTFILES} .dist/${DISTNAME}
	chmod 755 .dist/${DISTNAME}/configure
	cd .dist && tar zcf ../$@ ${DISTNAME}
	rm -rf .dist

# -- dependency management ---

# these .d files are produced during the first build if the compiler
# supports it.

-include log.d
-include lstun.d
