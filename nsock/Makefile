INCLUDE_DIR = ../include
CC = g++
CFLAGS = -I$(INCLUDE_DIR) -fsanitize=address -Wall -g -std=c++17

LIBS = -lpthread

DEPS = nsock.h npoll.h echoServer.h util.h commandServer.h

OBJ = nsock.o npoll.o util.o commandServer.o

GTESTOBJ = ../lib/libgtest.a

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: echoServer echoClient echoTest

echoServer: $(OBJ) echoServer.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

echoClient: $(OBJ) echoClient.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

echoTest: $(OBJ) echoTest.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(GTESTOBJ)

.PHONY: clean all

clean:
	rm -f *.o *~ core echoServer echoClient
