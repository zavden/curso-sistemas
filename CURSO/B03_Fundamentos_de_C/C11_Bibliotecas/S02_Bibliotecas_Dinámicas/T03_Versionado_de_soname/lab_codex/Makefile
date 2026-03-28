CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
RUNTIME_LIB = runtime/lib
RUNTIME_BIN = runtime/bin

all: v1_0 program_v1

$(RUNTIME_LIB) $(RUNTIME_BIN):
	mkdir -p $(RUNTIME_LIB) $(RUNTIME_BIN)

v1_0: calc_v1.o | $(RUNTIME_LIB)
	$(CC) -shared -Wl,-soname,libcalc.so.1 -o $(RUNTIME_LIB)/libcalc.so.1.0.0 calc_v1.o
	ln -sf libcalc.so.1.0.0 $(RUNTIME_LIB)/libcalc.so.1
	ln -sf libcalc.so.1 $(RUNTIME_LIB)/libcalc.so

v1_1: calc_v1_1.o | $(RUNTIME_LIB)
	$(CC) -shared -Wl,-soname,libcalc.so.1 -o $(RUNTIME_LIB)/libcalc.so.1.1.0 calc_v1_1.o
	ln -sf libcalc.so.1.1.0 $(RUNTIME_LIB)/libcalc.so.1
	ln -sf libcalc.so.1 $(RUNTIME_LIB)/libcalc.so

v2_0: calc_v2.o | $(RUNTIME_LIB)
	$(CC) -shared -Wl,-soname,libcalc.so.2 -o $(RUNTIME_LIB)/libcalc.so.2.0.0 calc_v2.o
	ln -sf libcalc.so.2.0.0 $(RUNTIME_LIB)/libcalc.so.2
	ln -sf libcalc.so.2 $(RUNTIME_LIB)/libcalc.so

program_v1: program_v1.c calc_v1.h | $(RUNTIME_BIN)
	$(CC) $(CFLAGS) program_v1.c -I. -L$(RUNTIME_LIB) -lcalc \
		-Wl,-rpath,'$$ORIGIN/../lib' -o $(RUNTIME_BIN)/program_v1

program_v2: program_v2.c calc_v2.h | $(RUNTIME_BIN)
	$(CC) $(CFLAGS) program_v2.c -I. -L$(RUNTIME_LIB) -lcalc \
		-Wl,-rpath,'$$ORIGIN/../lib' -o $(RUNTIME_BIN)/program_v2

calc_v1.o: calc_v1.c calc_v1.h
	$(CC) $(CFLAGS) -fPIC -c calc_v1.c -o calc_v1.o

calc_v1_1.o: calc_v1_1.c calc_v1.h
	$(CC) $(CFLAGS) -fPIC -c calc_v1_1.c -o calc_v1_1.o

calc_v2.o: calc_v2.c calc_v2.h
	$(CC) $(CFLAGS) -fPIC -c calc_v2.c -o calc_v2.o

clean:
	rm -rf runtime
	rm -f calc_v1.o calc_v1_1.o calc_v2.o

.PHONY: all clean v1_0 v1_1 v2_0 program_v1 program_v2
