CC=gcc
CFLAGS=-Wall -Wextra -std=c99

all: raid5

raid5: raid5.c
	$(CC) $(CFLAGS) raid5.c -o raid5

clean:
	rm -f raid5
