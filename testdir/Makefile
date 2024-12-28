
# This make file has only been tested on a RPI 3B+.
CC=gcc

SRC=testtigerlog.c

LDFLAGS=-g -L/usr/local/lib -L../../utils/libs -llogutils -lstrutils -lz -lpthread -lm
CFLAGS=-std=gnu99

CFLAGS += -g -Wall -O2 -I./ -I../../utils/incs

PRG=testtigerlog

all: $(PRG)

$(PRG): $(PRG).o
	# ctl stop testtigerlog
	$(CC) $(PRG).o -o $(PRG) $(LDFLAGS)
	cp $(PRG) ~/bin/$(PRG)

$(PRG).o: $(PRG).c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(PRG).o $(PRG)

