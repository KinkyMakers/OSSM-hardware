#include <stdint.h>

#include "PoorManFloat.h"

#ifdef TEST_TIMING
uint32_t calculate_ticks_v1(uint32_t steps, float acceleration);
uint32_t calculate_ticks_v2(uint32_t steps, float acceleration);
uint32_t calculate_ticks_v3(uint32_t steps, float pre_calc);
uint32_t calculate_ticks_v4(uint32_t steps, uint32_t acceleration);
uint32_t calculate_ticks_v5(uint32_t steps, upm_float pre_calc);
uint32_t calculate_ticks_v6(uint32_t steps, upm_float pre_calc);
uint32_t calculate_ticks_v7(uint32_t steps, upm_float pre_calc);
#endif
uint32_t calculate_ticks_v8(uint32_t steps, upm_float pre_calc);
#ifdef TEST
uint32_t calculate_ticks_v9(uint32_t steps, upm_float pre_calc);
#endif
