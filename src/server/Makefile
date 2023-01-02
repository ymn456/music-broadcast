CFLAGS += -I../include/    -pthread

DFLAGS += -lpthread

all:server

%.o:%.c
	$(CC)  -c -g $^ -o $@ $(CFLAGS) $(DFLAGS)

server: server.o  mytbf.o medialib.o thr_channel.o thr_list.o
	$(CC)  $^ -o $@ $(CFLAGS) $(DFLAGS)

clean:
	rm *.o server -rf