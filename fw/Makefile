CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra
BUILD := build
EMU := ../emu
SRC := afcounter.c hal_common.c regmenu.c app.c bargraph.c control.c format.c gpib.c help.c keys.c level.c meter.c modulation.c overload.c params.c readings.c rfcounter.c scope.c screen.c selftest.c service.c settings.c synth.c tones.c tx.c host/test_fw.c $(EMU)/machine.c $(EMU)/cpu8085.c

$(BUILD)/test_fw: $(SRC) level_table.h afcounter.h app.h bargraph.h control.h format.h gpib.h hal.h help.h keys.h level.h meter.h modulation.h overload.h params.h regs.h readings.h regmenu.h rfcounter.h scope.h screen.h selftest.h service.h settings.h synth.h tones.h tx.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm


$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) generated

.PHONY: clean
