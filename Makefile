# ABOUTME: Build, test and install targets for the fan tools (fanset, fanread, fankeys).
# ABOUTME: make check (strict build + unit + CLI), make test-e2e (read tools on real SMC), make test-hw (write path, sudo).

CFLAGS  ?= -O2
CFLAGS  += -std=gnu11 -Wall -Wextra -Werror
LDLIBS  := -framework IOKit
PREFIX  ?= /usr/local
BIN     := bin
TOOLS   := fanset fanread fankeys

.PHONY: help all check test test-unit test-cli test-e2e test-hw install uninstall clean

.DEFAULT_GOAL := help

help: ## Show this help
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "  %-12s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

all: $(TOOLS:%=$(BIN)/%) ## Build the three tools into bin/

$(BIN)/test_helpers: tests/test_helpers.c src/smc.c src/smc.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ tests/test_helpers.c src/smc.c $(LDLIBS)

$(BIN)/%: src/%.c src/smc.c src/smc.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ src/$*.c src/smc.c $(LDLIBS)

$(BIN):
	mkdir -p $@

check: test ## Build with warnings as errors and run unit + CLI tests (CI gate)

test: test-unit test-cli ## Run unit + CLI tests

test-unit: $(BIN)/test_helpers ## Unit tests for the hardware-free helpers
	$(BIN)/test_helpers

test-cli: all ## CLI tests: argument and permission handling, no hardware needed
	BIN=$(BIN) tests/test_cli.sh

test-e2e: all ## End-to-end read tests on the real SMC (skip where there are no fans)
	BIN=$(BIN) tests/test_e2e.sh

test-hw: all ## End-to-end write test on real fans (asks for sudo, restores auto mode)
	BIN=$(BIN) tests/test_hw.sh

install: all ## Install the tools to $(PREFIX)/bin (default /usr/local/bin, usually needs sudo)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TOOLS:%=$(BIN)/%) $(DESTDIR)$(PREFIX)/bin/

uninstall: ## Remove the installed tools
	rm -f $(TOOLS:%=$(DESTDIR)$(PREFIX)/bin/%)

clean: ## Remove build output
	rm -rf $(BIN)
