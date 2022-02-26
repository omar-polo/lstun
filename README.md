# lstun -- lazy ssh tunnel

lstun is a simple utility to lazily (on demand) spawn a ssh tunnel to
a remote machine and kill it after some time of inactivity.

Note that the the -main branch targets OpenBSD only; on other systems
please use the -portable branch.

To compile it just run

	$ make

The only dependency is libevent.  It expects ssh to be `/usr/bin/ssh`,
compile with `-DSSH_PATH=...` to alter the path eventually.


### Usage

```
usage: lstun [-dv] -B sshaddr -b addr [-t timeout] destination
```

Check out the manpage for further the usage.


### Motivation

It was written to forward lazily all the traffic on the local port
2525 to a remote port 25, thus using ssh as some sort of
authentication.

The need for the "lazy" opening and closing of the tunnel is to avoid
wasting resources when not needed.
