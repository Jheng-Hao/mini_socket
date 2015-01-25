SHELL = /bin/bash
CC = g++
CFLAGS = -O3
INC =
LIB = 

main:
	$(CC) -o $@ main.cc server.cc socket_manager.cc address.cc sctp_socket.cc -lpthread -lsctp -DDEBUG -DSOCKET_DEBUG

.PHONY: clean
clean:
	@rm -rf *.o
	@rm main
