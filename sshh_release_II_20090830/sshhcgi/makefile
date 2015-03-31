

all: sshhcgi


sshhcgi:
	g++ -O2 -Wall -Wno-write-strings  -I../Q -o sshhcgi ../Q/Q/socketio.cpp ../Q/Q/connector.cpp ../Q/Q/memm.cpp sshhcgi.cpp

clean:
	rm -f sshhcgi
