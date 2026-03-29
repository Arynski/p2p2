CC = gcc
CFLAGS = -Wall -Wextra -I.

COMMON_OBJS = common/network.o

SERVER_OBJS = serwer/main.o serwer/handler.o serwer/room.o $(COMMON_OBJS)
CLIENT_OBJS = klient/main.o klient/host.o klient/peer.o klient/handler.o klient/tui/tui.o $(COMMON_OBJS)
all: serwer klient

serwer: $(SERVER_OBJS)
	$(CC) -o serwer/egzek $(SERVER_OBJS)

klient: $(CLIENT_OBJS)
	$(CC) -o klient/egzek $(CLIENT_OBJS) -lncurses

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) serwer/egzek klient/egzek