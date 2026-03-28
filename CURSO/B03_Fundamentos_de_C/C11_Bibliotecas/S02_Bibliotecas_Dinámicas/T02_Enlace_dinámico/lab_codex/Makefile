CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

all: app/lib/libcalc.so app/bin/demo_plain app/bin/demo_runpath

app/lib/libcalc.so: app/lib/calc.c app/lib/calc.h
	$(CC) $(CFLAGS) -fPIC -shared app/lib/calc.c -o app/lib/libcalc.so

app/bin/demo_plain: app/bin/main.c app/lib/calc.h app/lib/libcalc.so
	$(CC) $(CFLAGS) -Iapp/lib app/bin/main.c -Lapp/lib -lcalc -o app/bin/demo_plain

app/bin/demo_runpath: app/bin/main.c app/lib/calc.h app/lib/libcalc.so
	$(CC) $(CFLAGS) -Iapp/lib app/bin/main.c -Lapp/lib -lcalc \
		-Wl,-rpath,'$$ORIGIN/../lib' -o app/bin/demo_runpath

clean:
	rm -f app/lib/libcalc.so
	rm -f app/bin/demo_plain app/bin/demo_runpath

.PHONY: all clean
