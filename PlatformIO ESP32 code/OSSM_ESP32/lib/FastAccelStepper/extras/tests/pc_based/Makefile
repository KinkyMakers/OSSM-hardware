
PRJ_ROOT=$(shell git rev-parse --show-toplevel)
CFLAGS=-DTEST -Werror -g -I$(PRJ_ROOT)/src
CXXFLAGS=-DTEST -Werror -g -DF_CPU=16000000 -I$(PRJ_ROOT)/src
LDLIBS=-lm -lc

TESTS=$(basename $(wildcard test_??.cpp))

test: $(TESTS) pmf_test rmc_test
	./rmc_test
	./pmf_test
	$(addsuffix &&,$(addprefix ./,$(TESTS))) echo "All tests passed"

LIB_H=FastAccelStepper.h PoorManFloat.h StepperISR.h \
	  RampGenerator.h RampConstAcceleration.h RampCalculator.h common.h
LIB_O=FastAccelStepper.o PoorManFloat.o StepperISR_test.o \
	  RampGenerator.o RampConstAcceleration.o RampCalculator.o StepperISR.o

SRC_LIB_H=$(addprefix $(PRJ_ROOT)/src/,$(LIB_H))

test_%: test_%.o $(LIB_O)
	gcc -o $@ $< $(LIB_O) $(LDLIBS)

test_%.o: test_%.cpp $(SRC_LIB_H) RampChecker.h stubs.h
	g++ -c $(CXXFLAGS) -o $@ $<

pmf_test: pmf_test.o PoorManFloat.o
pmf_test.o: pmf_test.cpp $(PRJ_ROOT)/src/PoorManFloat.h stubs.h test_03.h

rmc_test: rmc_test.o PoorManFloat.o RampCalculator.o
rmc_test.o: rmc_test.cpp $(PRJ_ROOT)/src/PoorManFloat.h $(PRJ_ROOT)/src/RampCalculator.h stubs.h test_03.h

FastAccelStepper.o: $(PRJ_ROOT)/src/FastAccelStepper.cpp $(SRC_LIB_H)
	$(COMPILE.cpp) $< -o $@

PoorManFloat.o: $(PRJ_ROOT)/src/PoorManFloat.cpp $(PRJ_ROOT)/src/PoorManFloat.h
	$(COMPILE.cpp) $< -o $@

RampGenerator.o: $(PRJ_ROOT)/src/RampGenerator.cpp $(SRC_LIB_H)
	$(COMPILE.cpp) $< -o $@

RampConstAcceleration.o: $(PRJ_ROOT)/src/RampConstAcceleration.cpp $(SRC_LIB_H)
	$(COMPILE.cpp) $< -o $@

RampCalculator.o: $(PRJ_ROOT)/src/RampCalculator.cpp $(SRC_LIB_H)
	$(COMPILE.cpp) $< -o $@

StepperISR.o: $(PRJ_ROOT)/src/StepperISR.cpp $(SRC_LIB_H)
	$(COMPILE.cpp) $< -o $@

StepperISR_test.o: StepperISR_test.cpp $(SRC_LIB_H)

VERSION=$(shell git rev-parse --short HEAD)

clean:
	rm *.o test_[0-9][0-9] *.gnuplot pmf_test rmc_test
