CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra
BUILD := build

all: $(BUILD)/emu2955 $(BUILD)/test_cpu

$(BUILD)/emu2955: main.c machine.c cpu8085.c machine.h cpu8085.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ main.c machine.c cpu8085.c

$(BUILD)/test_cpu: test_cpu.c cpu8085.c cpu8085.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_cpu.c cpu8085.c

test: $(BUILD)/test_cpu
	$(BUILD)/test_cpu

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
