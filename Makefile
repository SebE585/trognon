# SPDX-License-Identifier: MIT

CC      ?= cc
PKG     ?= pkg-config
CFLAGS  += -std=c11 -Wall -Wextra -Iinclude $(shell $(PKG) --cflags libusb-1.0)
LDLIBS  += $(shell $(PKG) --libs libusb-1.0)

SRC     := src/device_db.c src/serial_parse.c src/usb_probe.c src/chain.c src/cli.c
OBJ     := $(SRC:.c=.o)
BIN     := trognon

.PHONY: all clean test test-sanitize diagrams evidence-index

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: tests/test_device_db tests/test_serial tests/test_chain
	./tests/test_device_db
	./tests/test_serial
	./tests/test_chain

# Same tests, built with AddressSanitizer + UBSan (CI runs this one).
SAN_CFLAGS = -std=c11 -Wall -Wextra -Werror -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude
test-sanitize:
	$(CC) $(SAN_CFLAGS) -o tests/san_device_db tests/test_device_db.c src/device_db.c
	$(CC) $(SAN_CFLAGS) -o tests/san_serial tests/test_serial.c src/serial_parse.c
	$(CC) $(SAN_CFLAGS) -o tests/san_chain tests/test_chain.c src/chain.c src/device_db.c
	./tests/san_device_db && ./tests/san_serial && ./tests/san_chain

tests/test_device_db: tests/test_device_db.c src/device_db.c
	$(CC) -std=c11 -Wall -Wextra -Iinclude -o $@ tests/test_device_db.c src/device_db.c

tests/test_serial: tests/test_serial.c src/serial_parse.c
	$(CC) -std=c11 -Wall -Wextra -Iinclude -o $@ tests/test_serial.c src/serial_parse.c

tests/test_chain: tests/test_chain.c src/chain.c src/device_db.c
	$(CC) -std=c11 -Wall -Wextra -Iinclude -o $@ tests/test_chain.c src/chain.c src/device_db.c

clean:
	rm -f $(OBJ) $(BIN) tests/test_device_db tests/test_serial tests/test_chain tests/san_device_db tests/san_serial tests/san_chain; rm -rf tests/*.dSYM

# Diagrams: docs/diagrams/*.yaml -> docs/img/*.svg, built with paper-forge (maintainer-side; the SVGs are
# committed, so readers need nothing). Needs paper-forge's `trognon` charte (charte/trognon.tokens.yaml, `make tokens`).
PAPER_FORGE ?= $(HOME)/projects/pro/deeptech/paper-forge
diagrams:
	@for f in docs/diagrams/*.yaml; do n=$$(basename $$f .yaml); \
	  python3 $(PAPER_FORGE)/diagrams/build.py $$PWD/$$f --no-pdf >/dev/null && \
	  cp $(PAPER_FORGE)/diagrams/dist/$$n.svg docs/img/$$n.svg && echo "docs/img/$$n.svg"; done

evidence-index:
	python3 tools/evidence_index.py
