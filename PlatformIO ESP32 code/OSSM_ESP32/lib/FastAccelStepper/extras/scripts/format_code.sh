#!/bin/sh

PRJ_ROOT=`git rev-parse --show-toplevel`
VERSION=`git rev-parse --short HEAD`

FILES=`find ${PRJ_ROOT} -type f -name '*.ino' -or -type f -name '*.cpp' -or -type f -name '*.h'`

clang-format -style="{BasedOnStyle: Google, SortIncludes: false}" -i ${FILES}
echo ${VERSION}
sed -i -e 's/#define VERSION.*$$/#define VERSION "post-$(VERSION)"/' ${PRJ_ROOT}/examples/StepperDemo/StepperDemo.ino

