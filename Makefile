CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L

.PHONY: all test clean

all: libverifier.a

verifier.o: verifier.c verifier.h
	$(CC) $(CFLAGS) -c verifier.c -o verifier.o

libverifier.a: verifier.o
	ar rcs libverifier.a verifier.o

test_verifier: test_verifier.c verifier.c verifier.h
	$(CC) $(CFLAGS) -o test_verifier test_verifier.c verifier.c

test: test_verifier
	./test_verifier

clean:
	rm -f verifier.o libverifier.a test_verifier
