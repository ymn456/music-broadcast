CFLAGS += -I../include/ -Wall

all:client

client.o:client.c
	gcc -c -g client.c -o client.o $(CFLAGS)
client:client.o
	gcc $^ -o $@ $(CFLAGS)

clean:
	rm -rf *.o client
