#!/bin/sh

echo "build directories"

if [ "$GITHUB_WORKSPACE" != "" ]
then
	# Make sure we are inside the github workspace
	cd $GITHUB_WORKSPACE
fi

# Whatever this script is started from, cd to the top level
ROOT=`git rev-parse --show-toplevel`
cd $ROOT

pwd

# So create the pio_dirs-directory during the github action 
rm -fR pio_dirs
mkdir pio_dirs
for i in `ls examples`
do
	mkdir -p pio_dirs/$i/src
	cd pio_dirs/$i
	ln -s ../../extras/ci/platformio.ini .
	cd src
	FILES=`cd ../../../examples/$i;find . -type f`
	for f in $FILES;do ln -s ../../../examples/$i/$f .;done
	cd ../../..
done

# for espidf as of now, the src/* files need to be linked into the example build directory
rm -fR pio_espidf
mkdir pio_espidf
for i in `ls examples`
do
	mkdir -p pio_espidf/$i/src
	cd pio_espidf/$i
	ln -s ../../extras/ci/platformio.ini .
	cd src
	FILES=`cd ../../../examples/$i;find . -type f`
	for f in $FILES;do ln -s ../../../examples/$i/$f .;done
	FILES=`cd ../../../src/.;find . -type f`
	for f in $FILES;do ln -s ../../../src/$f .;done
	cd ../../..
done

# Make one directory to test PoorManFloat on simulator
mkdir pio_dirs/PMF_test
mkdir pio_dirs/PMF_test/src
cd pio_dirs/PMF_test
ln -s ../../extras/ci/platformio.ini .
cd src
#sed  -e 's/%d/%ld/g' <../../../tests/test_03.h >test_03.h
ln -s ../../../extras/tests/pc_based/test_03.h .
ln -s ../../../extras/tests/pc_based/PMF_test.ino PMF_test.ino
cd ../../..

ls -al pio*
