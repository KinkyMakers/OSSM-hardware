#ifndef STUBS_H
#define STUBS_H

#define PROGMEM
#define pgm_read_byte_near(x) (*(x))

// For inducing interrupts while testing
void noInterrupts();
void interrupts();
void inject_fill_interrupt(int mark);

#define _BV(x) 0
#define ISR(x) void x()
#define inline
#define micros() 0

#include <math.h>

#define abs(x) ((x) > 0 ? (x) : -(x))
#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define digitalWrite(a, b) \
  {}
#define pinMode(a, b) \
  {}

extern char TCCR1A;
extern char TCCR1B;
extern char TCCR1C;
extern char TIMSK1;
extern char TIFR1;
extern unsigned short OCR1A;
extern unsigned short OCR1B;

#define test(x, msg) \
  if (!(x)) {        \
    puts(msg);       \
    assert(false);   \
  };
#endif
