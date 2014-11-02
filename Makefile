CC := gcc
CFLAGS := -pedantic -Wall -Wextra

all:
	$(CC) commands.c $(CFLAGS) -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include/ -I/usr/include/loudmouth-1.0/ -std=c99 -lglib-2.0 -shared -DMODULES_ENABLE -o libcommands.so -fPIC

install: all
	install -D libcommands.so "$(DESTDIR)"/libcommands.so

.PHONY: install
