ifndef SILENCE
	SILENCE=0
endif

PRJ_ROOT=$(shell git rev-parse --show-toplevel)
TESTS=$(wildcard test_*)
TESTS_SD=$(wildcard test_*sd_*)

TEST_FILES=$(addsuffix /.tested,$(TESTS))
SD_SRC_DIRS=$(addsuffix /src/.dir,$(TESTS_SD))

SRC=$(wildcard ../../src/*)

test: $(TEST_FILES)

%/.tested:	$(SRC) run_avr %/expect.txt %/platformio.ini
	make SILENCE=$(SILENCE)  -C $(dir $@)

makefiles: $(addsuffix /Makefile,$(TESTS))

%/Makefile:
	cd $(dir $@); ln -s ../Makefile.test Makefile

links: $(SD_SRC_DIRS)

%/src/.dir:
	mkdir -p $(dir $@)
	cd $(dir $@); ln -sf $(PRJ_ROOT)/examples/StepperDemo/* .

run_avr: simavr/simavr/run_avr
	ln -s simavr/simavr/run_avr .

simavr/simavr/run_avr: simavr/simavr/sim/run_avr.c
	make -C simavr build-simavr

simavr/simavr/sim/run_avr.c:
	git clone https://github.com/gin66/simavr.git

clean:
	rm -fR */.pio */.tested */x.vcd */result.txt
	find . -type l -delete
	find . -type d -empty -delete
	make links makefiles
