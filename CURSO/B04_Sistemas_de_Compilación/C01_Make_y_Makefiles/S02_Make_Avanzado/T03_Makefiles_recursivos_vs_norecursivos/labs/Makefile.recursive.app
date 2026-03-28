# app/Makefile (recursive version)
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c17
LDFLAGS := -L../lib
LDLIBS  := -lcalc

TARGET := calculator
SRCS   := main.c
OBJS   := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -I../lib -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
