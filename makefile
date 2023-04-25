CFLAGS = -Wall -g

OBJECTS = queue.o ppos_core.o
PROGRAMS = pingpong-tasks1 pingpong-tasks2 pingpong-tasks3 pingpong-dispatcher

all: ${PROGRAMS}

debug:
	make CPPFLAGS='-DDEBUG' all

test_q: queue.o
	cc ${CFLAGS}   -o $@ $? tests/testafila.c

pingpong-tasks1: ${OBJECTS}
pingpong-tasks2: ${OBJECTS}
pingpong-tasks3: ${OBJECTS}
pingpong-dispatcher: ${OBJECTS}

queue.o: 	 queue.c queue.h
ppos_core.o: ppos_core.c ppos_data.h ppos.h

clean:
	-rm -f ${OBJECTS}

purge: clean
	-rm -f ${PROGRAMS} test_q
