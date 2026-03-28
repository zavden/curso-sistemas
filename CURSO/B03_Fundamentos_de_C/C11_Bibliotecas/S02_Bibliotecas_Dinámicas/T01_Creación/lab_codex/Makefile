CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

all: libstats_default.so libstats_hidden.so demo_default demo_hidden

stats_default.o: stats.c stats.h
	$(CC) $(CFLAGS) -fPIC -DBUILDING_STATS -c stats.c -o stats_default.o

stats_hidden.o: stats.c stats.h
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -DBUILDING_STATS -c stats.c -o stats_hidden.o

libstats_default.so: stats_default.o
	$(CC) -shared -o libstats_default.so stats_default.o

libstats_hidden.so: stats_hidden.o
	$(CC) -shared -o libstats_hidden.so stats_hidden.o

demo_default: main.c stats.h libstats_default.so
	$(CC) $(CFLAGS) main.c -L. -lstats_default -o demo_default

demo_hidden: main.c stats.h libstats_hidden.so
	$(CC) $(CFLAGS) main.c -L. -lstats_hidden -o demo_hidden

clean:
	rm -f stats_default.o stats_hidden.o
	rm -f libstats_default.so libstats_hidden.so
	rm -f demo_default demo_hidden

.PHONY: all clean
