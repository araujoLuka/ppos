# Lucas Correia de Araujo - GRR 20206150
# PingPongOS - PingPong Operating System
# 'makefile'
#
# Disciplina: Sistema Operacionais (CI1215)
# Professor: Carlos A. Maziero, DINF UFPR
#
# Abril de 2023
# Regras:
#   all ; debug ; tests ; tar ; clean ; purge

# Variaveis make
CFLAGS = -Wall -g
OBJECTS = queue.o ppos_core.o
PROGRAMS = $(patsubst %.c, %, $(wildcard pingpong*.c))
TESTS = $(patsubst %.c, %, $(wildcard tests/*.c))

# Regra para arquivos objeto
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regras principais
all: $(PROGRAMS)
$(PROGRAMS): $(OBJECTS)

# Regras para definir debug
debug: CFLAGS += -DDEBUG
debug: all

# Regras para gerar binarios com codigos de teste
tests: $(OBJECTS) $(TESTS) 
tests/%: tests/%.c *.h
	$(CC) $(CFLAGS) $(OBJECTS) $(@:%=%.c) -o $(@:tests/%=%)

tar:
	mkdir ppos
	cp $(wildcard *.c) $(wildcard *.h) makefile ppos/
	-tar -czvf ppos.tar.gz ppos
	-rm -r ppos/

# Regra para excluir arquivos temporarios
clean:
	-rm -f $(OBJECTS) *.tar.gz

# Regra para excluir todos os arquivos gerado pelo make
purge: clean
	-rm -f $(PROGRAMS)
	-rm -f $(TESTS:tests/%=%)
