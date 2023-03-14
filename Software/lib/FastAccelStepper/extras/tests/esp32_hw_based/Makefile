all: compile test

DEV=ttyUSB0

test: M1.test M7.test

%.test:
	MOTOR=$* ./seq_01.sh $(DEV)
	MOTOR=$* ./seq_02.sh $(DEV)
	MOTOR=$* ./seq_03.sh $(DEV)

compile:
	(cd ../../../pio_dirs/StepperDemo;rm -fR .pio; pio run -e esp32 -t upload --upload-port /dev/$(DEV))

clean:
	rm seq*.log
