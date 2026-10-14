# SPDX-License-Identifier: MIT

CC      ?= cc
PKG     ?= pkg-config
CFLAGS  += -std=c11 -Wall -Wextra -Iinclude $(shell $(PKG) --cflags libusb-1.0)
LDLIBS  += $(shell $(PKG) --libs libusb-1.0)

SRC     := src/device_db.c src/serial_parse.c src/usb_probe.c src/chain.c src/cli.c
OBJ     := $(SRC:.c=.o)
BIN     := trognon

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: tests/test_device_db tests/test_serial tests/test_chain
	./tests/test_device_db
	./tests/test_serial
	./tests/test_chain

tests/test_device_db: tests/test_device_db.c src/device_db.c
	$(CC) -std=c11 -Wall -Wextra -Iinclude -o $@ tests/test_device_db.c src/device_db.c

tests/test_serial: tests/test_serial.c src/serial_parse.c
	$(CC) -std=c11 -Wall -Wextra -Iinclude -o $@ tests/test_serial.c src/serial_parse.c

tests/test_chain: tests/test_chain.c src/chain.c src/device_db.c
	$(CC) -std=c11 -Wall -Wextra -Iinclude -o $@ tests/test_chain.c src/chain.c src/device_db.c

clean:
	rm -f $(OBJ) $(BIN) tests/test_device_db tests/test_serial tests/test_chain
