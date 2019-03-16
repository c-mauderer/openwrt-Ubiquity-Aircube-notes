ELF=ubnt-acb-fwgen
CFLAGS=-std=gnu11 -Wall -Wextra -g -O2

all: $(ELF)

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(BIN)

$(ELF): main.o crc32.o
	cc $(CFLAGS) -o $@ $^
