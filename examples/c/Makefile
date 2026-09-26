CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic

all: image_lab

image_lab: image_lab.c
	$(CC) $(CFLAGS) image_lab.c -lm -o image_lab

run: image_lab
	./image_lab generated

clean:
	rm -f image_lab
