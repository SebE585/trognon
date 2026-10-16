# SPDX-License-Identifier: MIT

CC      ?= cc
PKG     ?= pkg-config
CFLAGS  += -std=c11 -Wall -Wextra -Iinclude $(shell $(PKG) --cflags libusb-1.0)
LDLIBS  += $(shell $(PKG) --libs libusb-1.0)

SRC     := src/device_db.c src/serial_parse.c src/usb_probe.c src/chain.c src/cli.c
OBJ     := $(SRC:.c=.o)
BIN     := trognon

.PHONY: all clean test test-sanitize diagrams evidence-index docs-build docs-check

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
# Padding around each diagram, in pixels (d2's default is 100, which shrinks the text in a page column).
D2_PAD ?= 16
diagrams:
	@for f in docs/diagrams/*.yaml; do n=$$(basename $$f .yaml); \
	  D2_PAD=$(D2_PAD) python3 $(PAPER_FORGE)/diagrams/build.py $$PWD/$$f --no-pdf >/dev/null && \
	  cp $(PAPER_FORGE)/diagrams/dist/$$n.svg docs/img/$$n.svg && echo "docs/img/$$n.svg"; done

evidence-index:
	python3 tools/evidence_index.py

# Regenerate the IOP decompilation-coverage table from tools/iop_analysis/catalog/ (pilot,
# 2026-10-16). Never needs the analysed binaries or r2 -- the catalog's own tool_coverage.json
# files cache that, produced once locally.
iop-coverage: iop-coverage-test
	python3 tools/iop_analysis/gen_coverage.py

iop-coverage-test:
	python3 -m unittest tools.iop_analysis.test_gen_coverage -v
	python3 -m unittest tools.iop_analysis.test_extract_kext -v

# Build the site and run structural checks mkdocs --strict does not cover (external audit,
# 2026-10-16): currently, exactly one <h1> per page. Run this, not a bare `mkdocs build`,
# before trusting a docs change is safe to publish.
docs-build: evidence-index iop-coverage
	python3 tools/write_build_commit.py
	mkdocs build --strict

docs-check: docs-build
	python3 -I tools/check_headings.py
	python3 -I tools/check_internal_links.py
	git diff --exit-code -- docs/snippets/evidence-table.en.md docs/snippets/evidence-table.fr.md || \
	  { echo "docs-check: evidence-table.{en,fr}.md are stale -- run 'make evidence-index' and commit the result"; exit 1; }
	git diff --exit-code -- docs/snippets/iop-coverage.en.md || \
	  { echo "docs-check: iop-coverage.en.md is stale -- run 'make iop-coverage' and commit the result"; exit 1; }
