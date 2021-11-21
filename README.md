# lstun -- lazy ssh tunnel

lstun is a simple utility to lazily (on demand) spawn a ssh tunnel to
a remote machine and kill it after some time of inactivity.

Given its narrow scope, it's probably not useful to anybody but me, so
I haven't invested much time in making it easily portable.  Said that,
it shouldn't be hard to compile it in non-OpenBSD systems: you have
just to compile it by hand and (probably) provide an implementation
for strtonum(3) and getprogname(3).

To compile it just run

	make

The only dependency is libevent.

By default it uses `/usr/bin/ssh`, you can change it at compile time
with `-D SSH_PATH=<whatever>`.

Check out the manpage for the usage.
