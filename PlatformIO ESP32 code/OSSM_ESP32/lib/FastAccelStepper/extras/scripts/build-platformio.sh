#!/bin/sh

TARGETS=${1:-nanoatmega328 atmega2560 esp32 esp32s2 esp32c3 atmelsam atmega32u4}
echo "execute for ${TARGETS}"

if [ "$GITHUB_WORKSPACE" != "" ]
then
	# Make sure we are inside the github workspace
	cd $GITHUB_WORKSPACE
fi

# Whatever this script is started from, cd to the top level
ROOT=`git rev-parse --show-toplevel`
cd $ROOT

# install platformio, if needed
which pio
if [ $? -ne 0 ]
then
	# Install PlatformIO CLI
	export PATH=$PATH:~/.platformio/penv/bin
	curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
	python3 get-platformio.py

	# Use automated install from pio run
	# pio platform install "atmelavr"
	# pio platform install "atmelsam"
	# pio platform install "espressif32"
fi

set -e
for i in pio_dirs/*
do
	for p in ${TARGETS}
	do
		echo $p: $i
		(cd $i;pio run -s -e $p)
	done
done
