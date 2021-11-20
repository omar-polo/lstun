PROG =		lstun
LDADD =		-levent
WARNINGS =	yes

.ifdef DEBUG
CFLAGS =	-O0 -g
.endif

.include <bsd.prog.mk>
