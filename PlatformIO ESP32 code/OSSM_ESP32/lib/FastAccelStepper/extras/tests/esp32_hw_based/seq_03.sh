#!/bin/sh
TTY=${1:-ttyUSB0}
DEV="-d /dev/${TTY} -b 115200"
MOTOR=${MOTOR:-M1}

COMPLETE="test completed"
PASS="test passed"
MAX_RUN_S=300


for SEQ in 13 01 02 03 04 06 07 10 11
do
	LOG="$0_$SEQ.log"
	CMD="$MOTOR p7,0,0 t $MOTOR $SEQ R "

	echo "reset esp32"
	grabserial $DEV -c ' x reset ' -q "$MOTOR:" -e 10
	sleep 2

	echo "send commands"
	grabserial $DEV -c "$CMD" -q "$COMPLETE" -e $MAX_RUN_S -o $LOG
	echo

	if [ `gawk -f judge_pcnt_sync.awk $LOG | grep -c PASS` -ne 1 ]
	then
		grabserial $DEV -c 'r ' -q StepperDemo -e 1
		echo
		echo FAIL $0 pulse counter mismatch
		echo "test sequence $SEQ"
		exit 1
	fi

	if [ `grep -c "$PASS" $LOG` -eq 1 ]
	then
		echo PASS
	else
		grabserial $DEV -c ' x r ' -q StepperDemo -e 1
		echo
		echo "FAIL $0 result pattern: $PASS"
		echo "test sequence $SEQ"
		exit 1
	fi
done
