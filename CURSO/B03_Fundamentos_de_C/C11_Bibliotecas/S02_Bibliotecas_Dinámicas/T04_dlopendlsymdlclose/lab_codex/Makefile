CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

all: host plugin_upper

plugins:
	mkdir -p plugins

host: host.c plugin_api.h
	$(CC) $(CFLAGS) host.c -ldl -o host

plugin_upper: plugins | plugins_src/plugin_upper.c plugin_api.h
	$(CC) $(CFLAGS) -fPIC -shared plugins_src/plugin_upper.c -o plugins/plugin_upper.so

plugin_reverse: plugins | plugins_src/plugin_reverse.c plugin_api.h
	$(CC) $(CFLAGS) -fPIC -shared plugins_src/plugin_reverse.c -o plugins/plugin_reverse.so

clean:
	rm -f host
	rm -rf plugins

.PHONY: all clean plugins host plugin_upper plugin_reverse
