#include <stdint.h>
#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte_near(x) (*(x))
#endif
#include "PoorManFloat.h"
#ifdef TEST
#include <stdio.h>
#endif

#define UPM_FROM_PARTS(mantissa, exponent) \
  ((((uint16_t)exponent) << 8) | ((uint8_t)(mantissa)))

// new representation is:
//
//     76543210: 76543210
//     XXXXXXXX:1XXXXXXXX
//     exponent mantissa
//
// The mantissa represents encodes 256..511 and interpreted as 1.0 to ~2.0.
// The exponent uses an offset 128 to encode negative values and base 2
//
// 0x8000 => is 1
//
// Negative numbers and zero are not available
//
//
// I want to expound on the original comment, and include an example to make
// this less tedious for someone else.  You probably need to go lookup/refresh
// your memory on floating point formats to make this make sense.  In this
// format we will use an 8 bit exponent, and 8 bits of the data type as
// the mantissa, but as the diagram above shows, the first bit will always be 1
// So lets walk through an example.  For the Due, I needed the PWM clock in
// this format, so 21,000,000.
// First off, remember that floating point wants the entire number in the range
// of the mantissa, so we right shift the value until it is within the range of
// 256-511.  So for 21,000,000, thats 21000000 >> 16 = 0x140.  Remember that the
// leading one is hidden.  so the low byte is 0x40.  You'd be wrong to assume
// the exponent is simply 16.  again, in floating point, we want the mantissa
// to be treated as less than 1, so we shift past the bits.  This means we want
// it to look like 0.0x140, or 0.101000000.  Thats what mantissa means.  So, to
// get the correct value for the exponent, we need the exponent that makes this
// (as close to) the original value as we can get, that is 20,971,520 or
// 0001 0100 0000 0000 0000 0000 0000.  So again, we need to shift past our
// number, all the way to the above, so 0.101000000 << 25 is what we want.  Or
// 0x19, (remember we had 16 shifts, but 9 bits of mantissa including the
// hidden bit 16+9=25).  Now, we get a bit tricky.  We know the hidden bit is
// always there, so we don't include it in our exponent!  Thus, our exponent
// becomes 0x18 or 24.  So the almost final value is 0x1840, but we forgot
// about the sign bit.  We need to or 0x80 to the exponent to get the sign bit
// making for a final value of 0x9840 as seen in PoorManFloat.h.  To reverse
// this, we take our value of the mantissa enter it in calculator.  We take
// the exponent and subtract 8 from it.  Enter the mantissa with the added 1
// into a "programmer calculator", 0x140 and use the LSH operator to do 0x140
// << 16, and you should get 20,971,520.  There, now there's no need to be
// afraid of making new constants for the system to use different clock rates!
// I had to do this for the Due because the PWM clock could be divisions of 2
// off the main system clock of 84MHz.  I'd need a 5.25 divider to get 16 MHz.
// Thats not an even number, so even with the arbitrary clock divider, the best
// I could have managed was 16.8 MHz.  Easier to just use the modulo 4 divider
// and add the constants.
//
//
// rsqrt-tables
// ============
//
// The rsqrt-tables provide the direct solution for 1/sqrt(mantissa).
//
// rsqrt_exp_even is used, if the exponent is even.
// rsqrt_exp_odd is used, if the exponent is odd.
//
// For even exponents 1/sqrt(mantissa) translates in the range 1.0 down to
// 0.7077 The mantissa 256 (aka mantissa byte==0) is a special case to be
// treated separately, that's why this table has only 255 entries.
//
// The table is generated with this one liner:
//
// python:
//		[round(512.0/math.sqrt(i/256))-256 for i in range(257,512)
const PROGMEM uint8_t rsqrt_exp_even[255] = {
    255, 254, 253, 252, 251, 250, 249, 248, 247, 246, 245, 244, 243, 243, 242,
    241, 240, 239, 238, 237, 236, 235, 234, 234, 233, 232, 231, 230, 229, 228,
    228, 227, 226, 225, 224, 223, 223, 222, 221, 220, 219, 219, 218, 217, 216,
    215, 215, 214, 213, 212, 212, 211, 210, 209, 209, 208, 207, 206, 206, 205,
    204, 203, 203, 202, 201, 201, 200, 199, 198, 198, 197, 196, 196, 195, 194,
    194, 193, 192, 192, 191, 190, 190, 189, 188, 188, 187, 186, 186, 185, 184,
    184, 183, 183, 182, 181, 181, 180, 179, 179, 178, 178, 177, 176, 176, 175,
    175, 174, 173, 173, 172, 172, 171, 170, 170, 169, 169, 168, 168, 167, 166,
    166, 165, 165, 164, 164, 163, 163, 162, 162, 161, 160, 160, 159, 159, 158,
    158, 157, 157, 156, 156, 155, 155, 154, 154, 153, 153, 152, 152, 151, 151,
    150, 150, 149, 149, 148, 148, 147, 147, 146, 146, 145, 145, 144, 144, 143,
    143, 142, 142, 141, 141, 140, 140, 140, 139, 139, 138, 138, 137, 137, 136,
    136, 135, 135, 135, 134, 134, 133, 133, 132, 132, 131, 131, 131, 130, 130,
    129, 129, 128, 128, 128, 127, 127, 126, 126, 126, 125, 125, 124, 124, 123,
    123, 123, 122, 122, 121, 121, 121, 120, 120, 119, 119, 119, 118, 118, 118,
    117, 117, 116, 116, 116, 115, 115, 114, 114, 114, 113, 113, 113, 112, 112,
    111, 111, 111, 110, 110, 110, 109, 109, 109, 108, 108, 107, 107, 107, 106};
//
// For odd exponents, the exponent is reduced by 1 and the
// the mantissa multiplied by 2 and as such represents 2.0 to ~3
// Consequently 1/sqrt(mantissa) is in range 0.707107 down to ~0.5
//
// python:
//		[round(512.0/math.sqrt(2*i/256))-256 for i in range(256,512)]
const PROGMEM uint8_t rsqrt_exp_odd[256] = {
    106, 106, 105, 104, 103, 103, 102, 101, 101, 100, 99, 99, 98, 97, 97, 96,
    95,  95,  94,  94,  93,  92,  92,  91,  90,  90,  89, 89, 88, 87, 87, 86,
    86,  85,  84,  84,  83,  83,  82,  82,  81,  80,  80, 79, 79, 78, 78, 77,
    76,  76,  75,  75,  74,  74,  73,  73,  72,  72,  71, 71, 70, 70, 69, 69,
    68,  68,  67,  67,  66,  66,  65,  65,  64,  64,  63, 63, 62, 62, 61, 61,
    60,  60,  59,  59,  58,  58,  57,  57,  57,  56,  56, 55, 55, 54, 54, 53,
    53,  53,  52,  52,  51,  51,  50,  50,  50,  49,  49, 48, 48, 47, 47, 47,
    46,  46,  45,  45,  45,  44,  44,  43,  43,  43,  42, 42, 41, 41, 41, 40,
    40,  39,  39,  39,  38,  38,  38,  37,  37,  36,  36, 36, 35, 35, 35, 34,
    34,  34,  33,  33,  32,  32,  32,  31,  31,  31,  30, 30, 30, 29, 29, 29,
    28,  28,  28,  27,  27,  27,  26,  26,  26,  25,  25, 25, 24, 24, 24, 23,
    23,  23,  22,  22,  22,  21,  21,  21,  20,  20,  20, 19, 19, 19, 19, 18,
    18,  18,  17,  17,  17,  16,  16,  16,  16,  15,  15, 15, 14, 14, 14, 13,
    13,  13,  13,  12,  12,  12,  11,  11,  11,  11,  10, 10, 10, 9,  9,  9,
    9,   8,   8,   8,   8,   7,   7,   7,   6,   6,   6,  6,  5,  5,  5,  5,
    4,   4,   4,   4,   3,   3,   3,   3,   2,   2,   2,  2,  1,  1,  1,  1};

// The square table provides mantissa^2.
// For mantissa values indexing in the second part ot the table,
// the exponent has to be increased by 1
const PROGMEM uint8_t square_table[256] = {
    // python:
    //      [round((i/256)*(i/256) * 256)-256 for i in range(256,256+106)]
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 25, 27, 29, 31, 33, 35, 37, 39,
    42, 44, 46, 48, 50, 52, 55, 57, 59, 61, 64, 66, 68, 70, 73, 75, 77, 79, 82,
    84, 86, 89, 91, 93, 96, 98, 100, 103, 105, 107, 110, 112, 115, 117, 119,
    122, 124, 127, 129, 132, 134, 137, 139, 142, 144, 147, 149, 152, 154, 157,
    159, 162, 164, 167, 169, 172, 175, 177, 180, 182, 185, 188, 190, 193, 196,
    198, 201, 204, 206, 209, 212, 214, 217, 220, 223, 225, 228, 231, 234, 236,
    239, 242, 245, 247, 250, 253,
    // python:
    //      [round((i/256)*(i/256) * 128)-512 for i in range(256+106,256+256)]
    0, 1, 3, 4, 6, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23, 25, 26, 28, 29,
    31, 32, 34, 35, 37, 38, 40, 41, 43, 44, 46, 47, 49, 50, 52, 53, 55, 56, 58,
    60, 61, 63, 64, 66, 68, 69, 71, 72, 74, 76, 77, 79, 80, 82, 84, 85, 87, 89,
    90, 92, 93, 95, 97, 98, 100, 102, 103, 105, 107, 108, 110, 112, 114, 115,
    117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 134, 136, 138, 140, 141,
    143, 145, 147, 148, 150, 152, 154, 155, 157, 159, 161, 163, 164, 166, 168,
    170, 172, 174, 175, 177, 179, 181, 183, 185, 187, 188, 190, 192, 194, 196,
    198, 200, 202, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 223, 224,
    226, 228, 230, 232, 234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254};

// The reciprocal square table provides 1/mantissa^2.
// For mantissa values indexing in the second part ot the table,
// the exponent has to be decreased by 1
// Zero is a special case, so the table starts with offset 1
const PROGMEM uint8_t reciprocal_square_table[255] = {
    // python:
    //      [round(1/(i/256)/(i/256) * 512)-256 for i in range(256+1,256+107)]
    252, 248, 244, 240, 237, 233, 229, 225, 222, 218, 215, 211, 208, 204, 201,
    198, 194, 191, 188, 184, 181, 178, 175, 172, 169, 166, 163, 160, 157, 154,
    151, 149, 146, 143, 140, 138, 135, 132, 130, 127, 124, 122, 119, 117, 114,
    112, 109, 107, 105, 102, 100, 98, 95, 93, 91, 89, 87, 84, 82, 80, 78, 76,
    74, 72, 70, 68, 66, 64, 62, 60, 58, 56, 54, 52, 50, 48, 47, 45, 43, 41, 39,
    38, 36, 34, 33, 31, 29, 28, 26, 24, 23, 21, 19, 18, 16, 15, 13, 12, 10, 9,
    7, 6, 4, 3, 1, 0,
    // python:
    //      [round(1/(i/256)/(i/256) * 1024)-256 for i in
    //      range(256+107,256+256)]
    253, 250, 248, 245, 242, 240, 237, 234, 232, 229, 226, 224, 221, 219, 216,
    214, 211, 209, 206, 204, 201, 199, 197, 194, 192, 190, 187, 185, 183, 181,
    179, 176, 174, 172, 170, 168, 166, 163, 161, 159, 157, 155, 153, 151, 149,
    147, 145, 143, 141, 139, 137, 136, 134, 132, 130, 128, 126, 124, 123, 121,
    119, 117, 116, 114, 112, 110, 109, 107, 105, 104, 102, 100, 99, 97, 95, 94,
    92, 91, 89, 88, 86, 84, 83, 81, 80, 78, 77, 75, 74, 72, 71, 70, 68, 67, 65,
    64, 63, 61, 60, 58, 57, 56, 54, 53, 52, 50, 49, 48, 47, 45, 44, 43, 41, 40,
    39, 38, 36, 35, 34, 33, 32, 30, 29, 28, 27, 26, 25, 24, 22, 21, 20, 19, 18,
    17, 16, 15, 14, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

// The reciprocal table provides 1/mantissa
// Zero is a special case, so the table starts with offset 1
//
// python:
//      [round(1/(i/256) * 512)-256 for i in range(256+0,256+256)]
const PROGMEM uint8_t reciprocal_table[255] = {
    254, 252, 250, 248, 246, 244, 242, 240, 239, 237, 235, 233, 231, 229, 228,
    226, 224, 222, 221, 219, 217, 215, 214, 212, 210, 209, 207, 206, 204, 202,
    201, 199, 198, 196, 194, 193, 191, 190, 188, 187, 185, 184, 182, 181, 179,
    178, 177, 175, 174, 172, 171, 170, 168, 167, 165, 164, 163, 161, 160, 159,
    157, 156, 155, 154, 152, 151, 150, 149, 147, 146, 145, 144, 142, 141, 140,
    139, 138, 136, 135, 134, 133, 132, 131, 130, 128, 127, 126, 125, 124, 123,
    122, 121, 120, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107,
    106, 105, 104, 103, 102, 101, 100, 99,  98,  97,  96,  95,  94,  94,  93,
    92,  91,  90,  89,  88,  87,  86,  85,  84,  84,  83,  82,  81,  80,  79,
    78,  78,  77,  76,  75,  74,  73,  73,  72,  71,  70,  69,  68,  68,  67,
    66,  65,  64,  64,  63,  62,  61,  61,  60,  59,  58,  58,  57,  56,  55,
    55,  54,  53,  52,  52,  51,  50,  50,  49,  48,  47,  47,  46,  45,  45,
    44,  43,  43,  42,  41,  41,  40,  39,  39,  38,  37,  37,  36,  35,  35,
    34,  33,  33,  32,  31,  31,  30,  30,  29,  28,  28,  27,  26,  26,  25,
    25,  24,  23,  23,  22,  22,  21,  21,  20,  19,  19,  18,  18,  17,  16,
    16,  15,  15,  14,  14,  13,  13,  12,  11,  11,  10,  10,  9,   9,   8,
    8,   7,   7,   6,   6,   5,   5,   4,   4,   3,   3,   2,   2,   1,   1};

upm_float upm_from(uint8_t x) {  // TESTED
  uint16_t res;
  if ((x & 0xf0) == 0) {
    if ((x & 0x0c) == 0) {
      if ((x & 0x02) == 0) {
        x <<= 8;
        res = x;
      } else {
        x <<= 7;
        res = x | 0x0100;
      }
    } else {
      if ((x & 0x08) == 0) {
        x <<= 6;
        res = x | 0x0200;
      } else {
        x <<= 5;
        res = x | 0x0300;
      }
    }
  } else {
    if ((x & 0xc0) == 0) {
      if ((x & 0x20) == 0) {
        x <<= 4;
        res = x | 0x0400;
      } else {
        x <<= 3;
        res = x | 0x0500;
      }
    } else {
      if ((x & 0x80) == 0) {
        x <<= 2;
        res = x | 0x0600;
      } else {
        x <<= 1;
        res = x | 0x0700;
      }
    }
  }
  return res | 0x8000;
}
upm_float upm_from(uint16_t x) {  // TESTED
  if ((x & 0xff00) == 0) {
    return upm_from((uint8_t)x);
  }
  uint8_t exponent;
  if ((x & 0xf000) != 0) {
    if ((x & 0xc000) != 0) {
      if ((x & 0x8000) != 0) {
        x <<= 1;
        exponent = 0x8f;
      } else {
        x <<= 2;
        exponent = 0x8e;
      }
    } else {
      if ((x & 0x2000) != 0) {
        x <<= 3;
        exponent = 0x8d;
      } else {
        x <<= 4;
        exponent = 0x8c;
      }
    }
  } else {
    if ((x & 0x0c00) != 0) {
      if ((x & 0x0800) != 0) {
        x <<= 5;
        exponent = 0x8b;
      } else {
        x <<= 6;
        exponent = 0x8a;
      }
    } else {
      if ((x & 0x0200) != 0) {
        x <<= 7;
        exponent = 0x89;
      } else {
        x <<= 8;
        exponent = 0x88;
      }
    }
  }
  x >>= 8;
  return UPM_FROM_PARTS(x, exponent);
}
upm_float upm_from(uint32_t x) {  // TESTED
  if ((x & 0xffff0000) == 0) {
    return upm_from((uint16_t)x);
  }
  if ((x & 0xff000000) == 0) {
    uint16_t w = x >> 8;
    return upm_from(w) + 0x0800;
  } else {
    uint16_t w = x >> 16;
    return upm_from(w) + 0x1000;
  }
}
upm_float upm_multiply(upm_float x, upm_float y) {  // TESTED
  uint8_t mant_x = x & 255;
  uint8_t mant_y = y & 255;
  // the multiplication is
  //	(0x100+mant_x)*(0x100+,mant_y)
  //		= mant_x*mant_y+0x100*(mant_x+mant_y)+0x100*0x100
  //
  uint16_t xy = mant_x * mant_y;
  xy >>= 2;  // result is 0x10000..0x3fc01, so need to shift by 2
  xy += (mant_x + mant_y) << 6;  // add missing 0x100 multiplication
  xy += 0x4000;                  // add result of 0x100*0x100

  uint8_t mant;
  uint8_t exponent = (x >> 8) + (y >> 8) - 0x80;
  if ((xy & 0x8000) != 0) {
    mant = xy >> 7;
    exponent += 1;
  } else {
    mant = xy >> 6;
  }
  return UPM_FROM_PARTS(mant, exponent);
}
upm_float upm_square(upm_float x) {  // TESTED
  uint8_t mantissa = x & 0x00ff;
  uint8_t exponent = x >> 8;
  if (exponent >= 128) {
    exponent = (exponent - 64) << 1;
  } else {
    exponent = 128 - ((128 - exponent) << 1);
  }
  if (mantissa >= 106) {
    exponent++;
  }
  mantissa = pgm_read_byte_near(&square_table[mantissa]);
  return UPM_FROM_PARTS(mantissa, exponent);
}
upm_float upm_divide(upm_float x, upm_float y) {
  return upm_multiply(x, upm_reciprocal(y));
}
uint16_t upm_to_u16(upm_float x) {  // TESTED
  uint8_t exponent = x >> 8;
  if (exponent < 128) {
    return 0;
  }
  exponent -= 128;
  if (exponent > 15) {
    return 0xffff;
  }
  uint8_t mantissa = x & 0x00ff;
  uint16_t res = mantissa | 0x0100;
  if (exponent < 8) {
    res >>= (8 - exponent);
  } else {
    res <<= exponent - 8;
  }
  return res;
}
uint32_t upm_to_u32(upm_float x) {  // TESTED
  uint8_t exponent = x >> 8;
  if (exponent < 128) {
    return 0;
  }
  exponent -= 128;
  if (exponent > 31) {
    return 0xffffffff;
  }
  uint8_t mantissa = x & 0x00ff;
  uint32_t res = mantissa | 0x100;
  if (exponent < 8) {
    res >>= (8 - exponent);
  } else if (exponent > 8) {
    res <<= exponent - 8;
  }
  return res;
}
upm_float upm_shl(upm_float x, uint8_t n) { return x + (((uint16_t)n) << 8); }
upm_float upm_shr(upm_float x, uint8_t n) { return x - (((uint16_t)n) << 8); }
upm_float upm_rsqrt(upm_float x) {  // TESTED
  uint8_t mantissa = x & 0x00ff;
  uint8_t exponent = x >> 8;
  bool exp_even = (exponent & 1) == 0;
  if (exponent >= 128) {
    exponent = 191 - (exponent >> 1);
  } else {
    exponent = 127 + ((129 - exponent) >> 1);
  }
  if (exp_even) {
    if (mantissa == 0) {
      exponent++;
    } else {
      mantissa = pgm_read_byte_near(&rsqrt_exp_even[mantissa - 1]);
    }
  } else {
    mantissa = pgm_read_byte_near(&rsqrt_exp_odd[mantissa]);
  }
  return UPM_FROM_PARTS(mantissa, exponent);
}
upm_float upm_rsquare(upm_float x) {  // TESTED
  uint8_t mantissa = x & 0x00ff;
  uint8_t exponent = x >> 8;
  if (exponent >= 128) {
    exponent = 127 - ((exponent - 128) << 1);
  } else {
    exponent = 127 + ((128 - exponent) << 1);
  }
  if (mantissa == 0) {
    exponent++;
  } else {
    if (mantissa >= 107) {
      exponent--;
    }
    mantissa = pgm_read_byte_near(&reciprocal_square_table[mantissa - 1]);
  }
  return UPM_FROM_PARTS(mantissa, exponent);
}
upm_float upm_reciprocal(upm_float x) {  // TESTED
  uint8_t mantissa = x & 0x00ff;
  uint8_t exponent = x >> 8;
  exponent = 255 - exponent;
  if (mantissa == 0) {
    exponent++;
  } else {
    mantissa = pgm_read_byte_near(&reciprocal_table[mantissa - 1]);
  }
  return UPM_FROM_PARTS(mantissa, exponent);
}
