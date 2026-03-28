CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
AR = ar
ARFLAGS = rcs

all: report_default

portable_math.o: portable_math.c portable_math.h
	$(CC) $(CFLAGS) -c portable_math.c -o portable_math.o

main.o: main.c portable_math.h
	$(CC) $(CFLAGS) -c main.c -o main.o

libportablemath.a: portable_math.o
	$(AR) $(ARFLAGS) $@ portable_math.o

report_default: main.o libportablemath.a
	$(CC) main.o ./libportablemath.a -o $@

report_static: main.o libportablemath.a
	$(CC) -static main.o ./libportablemath.a -o $@

clean:
	rm -f portable_math.o main.o libportablemath.a report_default report_static

.PHONY: all clean
