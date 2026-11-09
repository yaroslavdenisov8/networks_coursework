CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

all: server

SOURCES = server.c worker.c client.c http.c net.c logger.c

server: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o server

clean:
	rm -f server
