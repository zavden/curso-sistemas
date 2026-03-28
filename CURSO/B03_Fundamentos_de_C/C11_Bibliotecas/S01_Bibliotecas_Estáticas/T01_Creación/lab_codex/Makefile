CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
AR = ar
ARFLAGS = rcs

OBJS = vec3.o vec3_io.o

all: demo_vec3

libvec3.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

demo_vec3: main.o libvec3.a
	$(CC) main.o -L. -lvec3 -lm -o $@

main.o: main.c vec3.h
	$(CC) $(CFLAGS) -c main.c -o $@

vec3.o: vec3.c vec3.h
	$(CC) $(CFLAGS) -c vec3.c -o $@

vec3_io.o: vec3_io.c vec3.h
	$(CC) $(CFLAGS) -c vec3_io.c -o $@

clean:
	rm -f $(OBJS) main.o libvec3.a demo_vec3

.PHONY: all clean
