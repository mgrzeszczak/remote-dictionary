CC=gcc
CFLAGS=-g -std=gnu99 -pedantic -Wall
LDLIBS= -lrt -lpthread -lm
TARGET=prog
FILES=prog.o dict.o server.o

${TARGET}: ${FILES}
	${CC} -o ${TARGET} ${FILES} ${LDLIBS}

prog.o: prog.c server.h
	${CC} ${CFLAGS} -o prog.o -c prog.c
dict.o: dict.c dict.h
	${CC} ${CFLAGS} -o dict.o -c dict.c
server.o: server.c dict.h server.h
	${CC} ${CFLAGS} -o server.o -c server.c
.PHONY: clean

clean:
	-rm -f ${FILES} ${TARGET}



