CFLAGS = -pedantic -Iinclude/
CFILES = src/main.c
CC = gcc

bin/fobfuscate: $(CFILES)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@
