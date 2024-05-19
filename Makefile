CC = gcc
CFLAGS = -ansi -Wall -g -O0 -Wwrite-strings -Wshadow \
	-pedantic-errors -fstack-protector-all -Wextra

.PHONY: all clean

all: cshell

clean:
	rm -f cshell *.o

cshell: cshell.o lexer.o parser.tab.o execute.o
	$(CC) -lreadline -o cshell cshell.o lexer.o parser.tab.o execute.o

cshell.o: cshell.c execute.h lexer.h
	$(CC) $(CFLAGS) -c cshell.c

lexer.o: lexer.c
	$(CC) $(CFLAGS) -c lexer.c

parser.tab.o: parser.tab.c command.h
	$(CC) $(CFLAGS) -c parser.tab.c

execute.o: execute.c execute.h command.h
	$(CC) $(CFLAGS) -c execute.c
