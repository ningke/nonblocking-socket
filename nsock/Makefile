INCLUDE_DIR = ../include
CC = g++
CFLAGS = -I$(INCLUDE_DIR) -fsanitize=address -Wall -g -std=c++17

LIBS=

DEPS = nsock.h npoll.h echoServer.h util.h

OBJ = nsock.o npoll.o util.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: echoServer echoClient

echoServer: $(OBJ) echoServer.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

echoClient: $(OBJ) echoClient.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean all

clean:
	rm -f *.o *~ core echoServer echoClient
