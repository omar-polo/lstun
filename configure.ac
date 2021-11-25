AC_INIT([lstun], [0.1.0], [op@omarpolo.com])
AC_CONFIG_LIBOBJ_DIR([compat])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS

AC_REPLACE_FUNCS([
	err \
	getprogname \
	strtonum \
])

AC_CHECK_LIB([event2], [event_init], [], [
	AC_CHECK_LIB([event], [event_init], [], [
		AC_MSG_ERROR([requires libevent])
	])
])

AM_CFLAGS="${AM_CFLAGS} -Wall -Wextra -Wmissing-prototypes"
AM_CFLAGS="${AM_CFLAGS} -Wstrict-prototypes -Wwrite-strings"
AM_CFLAGS="${AM_CFLAGS} -Wno-unused-parameter"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
