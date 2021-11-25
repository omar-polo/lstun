# lstun -- lazy ssh tunnel

lstun is a simple utility to lazily (on demand) spawn a ssh tunnel to
a remote machine and kill it after some time of inactivity.

To compile it just run

	$ ./autogen.sh	# if building from a git checkout
	$ ./configure
	$ make
	# make install	# eventually

The only dependency is libevent.

Check out the manpage for the usage.
