CFLAGS = -pedantic
CC = gcc

bin/fobfuscate: main.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@
