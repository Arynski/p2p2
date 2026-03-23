CC = gcc
CFLAGS = -Wall -Wextra 

OBJS = main.o handler.o network.o peer.o host.o

egzek: $(OBJS)
	$(CC) -o egzek $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f egzek $(OBJS)