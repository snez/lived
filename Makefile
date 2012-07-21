CC = g++
CCFLAGS = -O2

all: main.cpp Thread.o Client.o SocketHandler.o Server.o
	$(CC) $(CCFLAGS) -lpthread main.cpp Thread.o Client.o SocketHandler.o Server.o -o lived

Thread.o: Thread.cpp
	$(CC) $(CCFLAGS) -c Thread.cpp

Client.o: Client.cpp
	$(CC) $(CCFLAGS) -c Client.cpp

SocketHandler.o: SocketHandler.cpp
	$(CC) $(CCFLAGS) -c SocketHandler.cpp

Server.o: Server.cpp
	$(CC) $(CCFLAGS) -c Server.cpp

clean: 
	rm -f *.o lived

