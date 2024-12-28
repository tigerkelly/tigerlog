
# This make file has only been tested on a RPI 3B+.
CC=gcc

SRC=tigerlog.c

LDFLAGS=-g -L/usr/local/lib -L../utils/libs -lini -llogutils -lstrutils -lz -lpthread -lm
CFLAGS=-std=gnu99

CFLAGS += -g -Wall -O2 -I./ -I../utils/incs

PRG=tigerlog

all: $(PRG)

$(PRG): $(PRG).o
	ctl stop tigerlog
	$(CC) $(PRG).o -o $(PRG) $(LDFLAGS)
	cp $(PRG) ~/bin/$(PRG)
	ctl start tigerlog

$(PRG).o: $(PRG).c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(PRG).o $(PRG)

