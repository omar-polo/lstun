# lstun -- lazy ssh tunnel

lstun is a simple utility to lazily (on demand) spawn a ssh tunnel to
a remote machine and optionally kill it after some time of inactivity.

The only dependency is libevent.  It expects ssh to be `/usr/bin/ssh`,
compile with `-DSSH_PATH=...` to alter the path eventually.

To compile it just run

	$ ./configure
	$ make
	# make install # eventually

The build can be customized by passing arguments to the configure
script or by using a `configure.local` file; see `./configure -h` and
[`configure.local.example`](configure.local.example) for more
information.

The `configure` script can use pkg-config if available to find the
flags for libevent.  To disable the usage of it, pass
`PKG_CONFIG=false` to the configure script.

For Linux users with libbsd installed, the configure script can be
instructed to use libbsd instead of the bundled compats as follows:

	CFLAGS="$(pkg-config --cflags libbsd-overlay)" \
	    ./configure LDFLAGS="$(pkg-config --libs libbsd-overlay)"


### Usage

```
usage: lstun [-dv] -B sshaddr -b addr [-t timeout] destination
```

Check out the [manpage](lstun.1) for the usage.


### Motivation

It was written to forward lazily all the traffic on the local port
2525 to a remote port 25, thus using ssh as some sort of
authentication.

The need for the "lazy" opening and closing of the tunnel is to avoid
wasting resources when not needed.
