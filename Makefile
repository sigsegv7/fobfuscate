CFLAGS = -pedantic -Iinclude/
CFILES = src/main.c
ASMFILES = src/sse_accel.S
CC = gcc

bin/fobfuscate: $(CFILES) $(ASMFILES)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@
