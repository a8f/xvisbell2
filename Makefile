CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=gnu99
LFLAGS=-lX11

xvisbell: xvisbell.o
	gcc $(CFLAGS) $(LFLAGS) -o xvisbell xvisbell.o

xvisbell.o: xvisbell.c
	gcc $(CFLAGS) -c xvisbell.c

install: xvisbell
	install xvisbell /usr/bin/

clean:
	rm xvisbell.o xvisbell
