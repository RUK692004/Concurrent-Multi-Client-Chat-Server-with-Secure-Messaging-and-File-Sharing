CC=gcc
CFLAGS=-Wall
OBJS=server.o utils.o auth.o rooms.o commands.o

all: server client

server: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o server -lpthread

server.o: server.c utils.h auth.h rooms.h commands.h
	$(CC) $(CFLAGS) -c server.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

auth.o: auth.c auth.h
	$(CC) $(CFLAGS) -c auth.c

rooms.o: rooms.c rooms.h utils.h
	$(CC) $(CFLAGS) -c rooms.c

commands.o: commands.c commands.h rooms.h utils.h
	$(CC) $(CFLAGS) -c commands.c

client: client.c
	$(CC) $(CFLAGS) client.c -o client -pthread

clean:
	rm -f server client *.o