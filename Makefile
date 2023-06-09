CC = gcc
CFLAGS = -Os -std=c2x -Wall -g -fanalyzer
LDFLAGS = -lncurses -ltinfo

all:
	ib tc.c.ib
	gcc tc.c -o tc $(LDFLAGS)
clean:
	rm -rf *.c *.h tc
