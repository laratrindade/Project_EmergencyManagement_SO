all: so run

so: proj.o
	 gcc proj.c -lpthread -D_REENTRANT -Wall -o proj

run: so
	./proj
