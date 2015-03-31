
all: bin bin/sshh bin/sshhd bin/sshhcgi

bin:
	mkdir bin

bin/sshh: 
	cd sshh;make;mv sshh ../bin;cp sshh.cf ../bin

bin/sshhd:
	cd sshhd;make;mv sshhd ../bin;cp sshhd.cf ../bin

bin/sshhcgi:
	cd sshhcgi;make;mv sshhcgi ../bin;cp sshhcgi.cgi ../bin

