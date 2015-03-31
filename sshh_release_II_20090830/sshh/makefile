

all: sshh


sshh:
	g++ -O2 -Wall -Wno-write-strings -I../Q -I.  -o sshh ../Q/Q/base64.cpp ../Q/Q/socketio.cpp ../Q/Q/connector.cpp ../Q/Q/memm.cpp sshh.cpp

solarissshh:
	g++ -O2 -Wall -Wno-write-strings -I../Q -I. -lsocket -lnsl  -o sshh ../Q/Q/socketio.cpp ../Q/Q/connector.cpp ../Q/Q/memm.cpp sshh.cpp

clean:
	rm -f sshh
