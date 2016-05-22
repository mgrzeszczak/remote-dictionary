CC=gcc
CFLAGS=-g -pedantic -std=gnu99 -Wall
LDLIBS= -lrt -lpthread -lm

all: client

client: client.c
	$(CC) $(CFLASG) -o client client.c $(LDLIBS)
