#include <stdint.h>

#include "PoorManFloat.h"

//
// This file can be renamed to a .ino and compiled as sketch to be run on the
// target e.g. arduino nano.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define trace puts
#define xprintf printf
#define test(x, msg) \
  if (!(x)) {        \
    puts(msg);       \
    assert(false);   \
  };

unsigned int error_cnt = 0;

#include "test_03.h"

int main() {
  if (perform_test()) {
    xprintf("TEST_03 PASSED\n");
  }
}
