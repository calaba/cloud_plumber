#!/bin/sh
# sshhcgi.cgi. This is the script you put in your cgi-bin directory that is pointed to in sshh.cf (endcaller)
# it must be a separate script because you have to pass parameters to the sshhcgi binary.
# There's no way to pass command line params to a program from a web hit and 
# passing the passphrase as part of the request would defeat the purpose.
# So you might also want to limit the permissions on this cgi.

# Forward cgi request to sshhd
# Must pass 3 params to sshhcgi binary, host and port sshhd is listening on and
# a passphrase to encrypt with. If there are spaces in the passphrase, it must be quoted
# to appear as one parameter to the program.
# passphrase must match setting in sshh.cf
/usr/local/bin/sshh/sshhcgi 127.0.0.1 8888 "this is the end"