.PHONY: all clean

BINS=elf fat pt
CFLAGS=-std=c2x

all: $(BINS)

elf: elf.c
	$(CC) $(CFLAGS) -o $@ $^

fat: fat.c
	$(CC) $(CFLAGS) -o $@ $^

pt: pt.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(BINS)
