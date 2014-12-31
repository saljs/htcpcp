CC=gcc
CFLAGS=--std=gnu99
LDFLAGS=-lwiringPi 

all: coffee htcpcpd

coffee:
	$(CC) $(CFLAGS) coffee.c -o coffee
	
htcpcpd:
	$(CC) $(CFLAGS) $(LDFLAGS) htcpcpd.c -o htcpcpd

install:
	cp coffee /usr/local/bin/; cp htcpcpd /usr/local/bin/
	
clean:
	rm -f coffee htcpcpd
