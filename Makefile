CFLAGS = -I.

SRCS = boot.c sahara.c
HEADERS = i9305.h sahara.h

CC = gcc -Wall -Wextra

modem-boot: boot.o sahara.o
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
