CFLAGS = -Wall -Wextra -g

OBJECTS = queue.o testafila.o
PROGRAM = main

all: ${PROGRAM}

main: ${OBJECTS}
	gcc -o ${PROGRAM} ${CFLAGS} ${OBJECTS}

queue.o: queue.c queue.h
	gcc -c queue.c ${CFLAGS}

testafila.o: testafila.c
	gcc -c testafila.c

clean:
	-rm -f ${OBJECTS}

purge: clean
	-rm -f ${PROGRAM}
