all: logger client
logger: logger.c
	gcc logger.c -o logger -lrt -lpthread
client: client.c
	gcc client.c -o client -lrt -lpthread

clean:
	rm -f logger client