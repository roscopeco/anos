CFLAGS=-I../../system/include

.PHONY: all clean

all: mkramfs

clean:
	$(RM) *.o mkramfs

ramfs.o: ../../system/ramfs.c
	$(CC) $(CFLAGS) -c -o $@ $^

mkramfs: main.o ramfs.o
	$(CC) $(CFLAGS) -o $@ $^

