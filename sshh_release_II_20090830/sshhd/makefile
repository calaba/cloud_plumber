
all: sshhd

sshhd:
	g++ -O2 -Wall -Wno-write-strings -I../Q  -o sshhd ../Q/Q/socketio.cpp ../Q/Q/connector.cpp ../Q/Q/memm.cpp sshhd.cpp

clean:
	rm -f sshhd