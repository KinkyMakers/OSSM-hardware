#include "StepperISR.h"
//#ifndef ARDUINO_ARCH_SAM
//#define ARDUINO_ARCH_SAM
//#endif
//#define KEEP_SCORE
#if defined(ARDUINO_ARCH_SAM)
const bool enabled = false;
uint32_t junk = 0;
/*
I went through a ton of iterations of trying to make this work sd desired.
It seems quite simple, set the pulse period of the PWM generator, start it,
stop it after e->ticks number of ticks.  Except for that its never easy.

We start with how to detect that e->ticks has happened. I found that
regaurdless of output mode, the PIO device is always connected and receiving
inputs!  This means I can use it to trigger on a pulse being sent!  But this
means I will get an interrupt for every single pulse.  Thats not ideal, and I
wasn't sure this would be able to operate fast enough in this mode.  I
actually did what I could to get into the PWM shutdown as fast as possible.
However, it seems like it would still send one more pulse every time.  It
wouldn't register a full rise on my scope, and it probably....wouldn't have
triggered a pulse on a stepper driver...but probably isn't good enough.  No
matter what I did, I wasn't fast enough.  So I found that using the PIO to
disconnect the output of the pulse generator was nearly instantaneous.  So
fast that to keep the interrupt on a rising edge, I need to delay a few
microseconds to keep from chopping off the pulse!  In fact, my driver says
it needs 5 microsecond pulses.  I'm currently sending 10 microsencond pulses.
With a 5 microsecond delay before shutting off, the pulse is only 7
microseconds in length!  The performance of the code seems quite good!  So
not only is the overhead of the interrupts low enough to chop pulses off,
its good enough to easily manage at least one stepper.  (I need to make code
more generic and make ISRs for the other ports and test with more).

The last "trick" that was necessary was a delay in starting the pulse generator
after setting the direction pin.  Currently my test just spins the motor 1
revolution then spins it the other way.  So every stop needs a delay.  The
delay is 100% necessary for setting the direction pin, so it cannot simply be
removed!

With that, I've spun this motor to about 1600 RPM.  No matter how slow the
acceleration, it seems to stop somewhere just above that.  So the code can do
more than my nema23 test motor :)

Last "trick".  I didn't think about the fact that at 21MHz with a 16 bit
counter, the longest the system can delay is around 3ms.  Fortunately, this was
not lost on the original author.  the hasSteps or steps==0 case is when you
want to pause for a time (specified in the ticks member).  To pause, I can
disconnect the PWM peripheral, and  generate PWM interrupts instead of using
the PIO interrupts.  It may seem like a good idea to just use the PWM
interrupts.  However, this arrangement actually eliminates a branch, and keeps
pause code seprate from pulse code.  On top of that, I get a true count of
pulses output, rather than pulses attempted to be generated even if it was
disconnected.  Its actually more accurate than my scope, as if I am in
process of disconnecting the PWM peripheral, I still get a PIO interrupt, but
on the scope, I see only a small .8-.9V pulse.  Its possible that pulse could
be detected, though not likely.  Better to eliminate it if possible (which it
was!)  The branch is eliminated by not needing to check if it was the cmpr
register interrupt, or the period update interrupt.  The PWM interrupt is
always the period update, and the PIO is always the cmp.  One branch isn't
much, but its something!


I think I'm going about this all wrong.  I think I can set the AB reg to the PWM
generator, but set the output to 0 and still get interrupts on the PIO when a
pulse would have happened if we hadn't overridden the output to 0.  Look into
this...
*/
#include "FastAccelStepper.h"
inline void disconnectPWMPeriphal(Pio* port, uint8_t pin, uint32_t channelMask);

typedef struct _PWMCHANNELMAPPING {
  uint8_t pin;
  uint32_t channel;
  Pio* port;
  uint32_t channelMask;
} PWMCHANNELMAPPING;

#define NUM_PWM_CHANNELS 8
uint8_t TimerChannel_Map[NUM_PWM_CHANNELS];

uint8_t numChannels = 0;

// this is needed to give the background task isr access to engine
static FastAccelStepperEngine* fas_engine = NULL;

// Here are the global variables to interface with the interrupts
#if defined(KEEP_SCORE)  // If you want to count how many pulses we detect...
volatile uint32_t totalPulsesDetected[NUM_QUEUES];
volatile uint32_t totalSteps[NUM_QUEUES];
inline void IncrementQueue(int queue) { totalPulsesDetected[queue]++; }
inline void AddToTotalSteps(int queue, uint32_t steps) {
  totalSteps[queue] += steps;
}
#else
#define IncrementQueue(Q)
#define AddToTotalSteps(Q, N)
#endif
bool channelsUsed[8] = {false, false, false, false, false, false, false, false};
StepperQueue fas_queue[NUM_QUEUES];
PWMCHANNELMAPPING gChannelMap[NUM_QUEUES];

void TC5_Handler() {
  uint32_t SR0 = TC1->TC_CHANNEL[2].TC_SR;
  if (SR0 & TC_SR_CPCS) {
    // We're going to use TC5 as its not connected to any pins on the DUE to run
    // our engine... We only get in here on an RC compare, so if we're in here,
    // run manage steppers...
    fas_engine->manageSteppers();
    TC1->TC_CHANNEL[2].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;  // CLKEN and SWTRG
  }
}
// Handy function to strobe a pin, and will do so conditionally based on an int
// and a desired int.  I was using PWM channels to decide what to strobe, thus
// the variable names.  If you want to just strobe a pin leave the ints blank
void strobePin(uint32_t pin, uint32_t desiredChannel = 0,
               uint32_t channel = 0) {
  if (channel == desiredChannel) {
    digitalWrite(pin, HIGH);
    digitalWrite(pin, LOW);
  }
}
void PWM_Handler(void) {
  // The datasheet is INCREDIBLY confusing here again.  It writes of interrupts
  // on comparison registers, and tells us that each channel has comparison
  // registers, and proceeds to tell us that interrupts are only for synchronous
  // channels.  This is contradictory at best, useless at worst!  The truth is
  // that Atmel's tech writers are atrocious.  The PWM unit containts two almost
  // distinct peripherals.  Theres the PWM generator with its 8 channels and
  // individual register and clock selection.  In addition to this, attached to
  // channel 0 only is the "comparison" peripheral.  Even though the main PWM
  // channel registers will compare values against the clock, do not call them
  // comparison registers.  They are Period and Duty cycle registers.  Thats all
  // they can be used for, so don't think of them as "comparison", especially
  // since there are "comparison" registers which are only good for comparison
  // attached to channel 0.  The comparison unit is used to generate "special"
  // interrupts, all controleld by the IXR2 registers (where X can be E, D, M,
  // or S to make up the entire register set).  The fact they are tied to
  // channel 0 is why there is the confusion mention of "synchronous" in the
  // datasheet, despite synchronous being used to mean tying main PWM output
  // channels to the same clock so that either their left sides are aligned or
  // their centers are aligned (depending on a register setting).
  //
  // The IXR1 (again E,D,M,and S) registers are for the main PWM channels
  // only, and do not make use of the comparison unit.  They have 2 possible
  // sources, channel fault, and "clock event" was used because there are two
  // possible modes for the clock to be run in, but in the description, this
  // isn't explained.  It really ought to be called "Period Reset".  Effectively
  // this is what it is whether in left aligned or center aligned modes.  In
  // the timing diagrams on page 980 of the data sheet, its clear that it
  // responds directly to the period register, not the clock itself.  The clock
  // also responds to the period register, so naming of this interrupt/event
  // would make *FAR* more sense to be tied to the register than effects it the
  // most!  So without checking the datasheet, its an interrupt when the period
  // expires.  In center aligned the counter counts up until it matches the
  // period register, then back to 0 to make "one full period".  When it hits 0
  // the "Counter Event" interrupt for the channel "CHIDx" is fired, again,
  // think of it as Period Reset interrupt.  In left Aligned the counter counts
  // up until it reaches the period register and then resets to 0, and fires
  // the "CHIDx" interrupt" (period reset interrupt).  That was way more
  // description than should have been necessary, but I wanted to clear up the
  // nearly worthless datasheet.  It is supposed to be a reference, you're not
  // supposed to need to read the entire thing, correleate all discrepencies,
  // determine the truth, then apply, it should be a reference meaning each
  // section should be able to be read/understood individually.  Intel does a
  // fantastic job of this with IA32 Software Developer's Manual!  Atmel could
  // take a lesson!  Even AMD with Radeon GPU documenation was better than this!

  // Anyhow, this ISR without the complexity of IXR2 registers is actually quite
  // simple.  Figure out which channel had a period reset event, and service it.
  // Since we have to read the entire ISR1 register, we need to make sure we
  // don't have multiple period update events.  It should be rare, but it can
  // happen! Then, because we only use this for pausing, and each pause has one
  // period cycle, we simply advance rp, and decide what to do from there (pause
  // again or switch to outputting steps).  Pretty easy :)

  uint32_t sr = PWM_INTERFACE->PWM_ISR1;
  // uint32_t sr2 = PWM_INTERFACE->PWM_ISR2;
  uint8_t leading_zeros;
  sr = sr & 0xFF;  // Ignore the faults, they will screw this up, and we
                   // shouldn't be getting faults...
  while ((leading_zeros = __CLZ(sr)) < 32) {
    uint8_t channel = 32 - leading_zeros - 1;
    uint32_t mask = (1 << channel);
    sr = sr & (~mask);
    uint8_t queue_num = TimerChannel_Map[channel];
    StepperQueue* q = &fas_queue[queue_num];
    // if (q->_skipNextPWMInterrupt)
    // The idea of just blindly telling the microcontroller to skip an
    // interrupt is not possible.  It should be totally deterministic to the
    // code, but it isn't.  Something else is going on under the hood masked
    // from "userland" code.  For some reason, one channel or the other
    // generates a spurious interrupt, but rarely do both.  This makes no
    // sense.  They are supposed to be independent.  So, rather than just
    // saying, this is my first time re-enabling the interrupt, and its going
    // to fire, so skip it, I instead need to do some math on execution time.
    // If we're really close to when the interrupt was enabled, we should skip.
    // I've "guessed" at 100 microseconds.  This is irrelevant to high rate
    // pulses, so we're only concered with pauses, which are going to be
    // measured in milliseconds, so 100 microseconds ought to be really close!
    // but 1/10th of a millisecond shouldn't warrant a delay.  Seems like a
    // good balance.  Hopefully this strategy proves correct for all cases...
    uint32_t t = micros();
    uint32_t timeElapsed = micros() - q->timePWMInterruptEnabled;
    if (t < q->timePWMInterruptEnabled)  // Timer wrapped...
    {
      timeElapsed = 0xFFFFFFFF - q->timePWMInterruptEnabled;
      timeElapsed += t;
    }
    if (timeElapsed < 100) {
      // This is unfortunately still entirely necessary.  I had thought I
      // could gate this in other ways.  Nope, the ISR is low lag enough we
      // easily get into this ISR immediatly after enabling it for the pulse
      // that was just handled by the PIO ISR, so we still need to skip one.
      // Being too fast is a good thing :)
      continue;
    }
    if (!q->_pauseCommanded) {
      // In the immortal words of Richard Gray aka levelord,
      //"YOU'RE NOT SUPPOSED TO BE HERE!"

      // try disabling the interrupt again...I saw on the logic analyzer
      // we got in here a good 5 times extra!  Seems like its not always
      // disabling when we tell it to.  Based on the quality of the datasheet
      // I'm not altogether surprised!  Handle it and move on...
      PWM_INTERFACE->PWM_IDR1 = mask;
      continue;
    }
    PWMCHANNELMAPPING* mapping = &gChannelMap[queue_num];
    q->driver_data = (void*)mapping;

    Pio* port = mapping->port;

    // Now with the queue, we can get the current entry, and see if we need to
    uint8_t rp = q->read_idx;
    if (rp == q->next_write_idx) {
      // I believe this is solved by the gating of _delayCommanded and
      // hasISRActive, but its not a bad idea to double check!
      // Double interrupt case, bail before we destroy the read index...
      // We're going to write rp and wp out as pulses onto pin32.
      continue;
    }
    rp = ++q->read_idx;
    struct queue_entry* e = &q->entry[rp & QUEUE_LEN_MASK];
    if (rp == q->next_write_idx) {
      q->_hasISRactive = false;
      // Since we arent sure about more pauses, we need to say no we aren't
      // pausing anymore...
      q->_pauseCommanded = false;
      // We're disconnected already, so we don't need to worry about that
      // disconnect the interrupt though.
      PWM_INTERFACE->PWM_IDR1 = mask;
      PWM_INTERFACE->PWM_DIS = mask;
      disconnectPWMPeriphal(port, mapping->pin, mapping->channelMask);
      PWM_INTERFACE->PWM_IDR1 = mask;
      q->read_idx = rp;
      pinMode(mapping->pin, OUTPUT);
      PIO_SetOutput(mapping->port, g_APinDescription[mapping->pin].ulPin, 0, 0,
                    0);

      continue;  // We're done apparently
    }
    if (e->steps > 0) {
      // stop the interrupt on the PWM generator, set the period,
      // re-attach the PIO handler, disconnect the  PWM generator
      // interrupt, and send it on its way...One problem, we have to use
      // this ordering, but that means we miss an interrupt.  So
      // decrement the ticks here.

      // This delay is absolutely necessary.  Our interrupt latency is
      // quite low compared to the period of the PWM output.  So we're able
      // to get in here, determine our pause is done, switch the pin back to
      // the PWM output, and output the pulse before it goes low, LONG before
      // it goes low.
      // So we add a 10 microsecond delay knowing the delay will be longer
      // than 10 microseconds to ensure we have passed the point where the
      // pulse is high!  We won't be far off, so we aren't killing the processor
      // with ISR time.  If there were 2+ cores, I'd have another thread that
      // would do the waiting so the ISR could be as short as possible, but in
      // reality,10  microseonds would probably be shorter than the time it
      // would take to setup a timer channel, enable its interrupt, and get into
      // the ISR, so we'd likely be *less* efficient than just delaying.
      delayMicroseconds(10);
      PWM_INTERFACE->PWM_CH_NUM[channel].PWM_CPRDUPD =
          e->ticks;  // Change the period first!
      PWM_INTERFACE->PWM_IDR1 = mask;
      uint32_t dwSR = port->PIO_ABSR;
      port->PIO_ABSR = (g_APinDescription[mapping->pin].ulPin | dwSR);
      // Remove the pins from under the control of PIO
      port->PIO_ODR |= g_APinDescription[mapping->pin].ulPin;
      port->PIO_PDR = g_APinDescription[mapping->pin].ulPin;
      port->PIO_IER = g_APinDescription[mapping->pin].ulPin;
      q->_pauseCommanded = false;
    } else {
      // We needed to pause longer, so set the period, and wait...
      // set the period, reconnect, restart, then disconnect PWM interrupt, so
      // the next interrupt goes to the PIO interrupt handler and works like
      // normal
      PWM_INTERFACE->PWM_CH_NUM[channel].PWM_CPRDUPD = e->ticks;
    }
    q->read_idx = rp;
  }
}

inline void disconnectPWMPeriphal(Pio* port, uint8_t pin,
                                  uint32_t channelMask) {
  // This function has become quite minimal.  Its still somewhat nice to have
  // it seperated out, but I may decided it needs to just be inline...2 lines
  // saved and function call overhead added :-/
  PWM_INTERFACE->PWM_DIS = channelMask;
  PIO_SetOutput(port, g_APinDescription[pin].ulPin, 0, 0, 0);
}

// gin66: removed the obsolete clearISR flag
inline void attachPWMPeripheral(Pio* port, uint8_t pin, uint32_t channelMask) {
  PWM_INTERFACE->PWM_IDR1 = (channelMask);
  uint32_t dwSR = port->PIO_ABSR;
  port->PIO_ABSR = (g_APinDescription[pin].ulPin | dwSR);
  // Remove the pins from under the control of PIO
  port->PIO_ODR |= g_APinDescription[pin].ulPin;
  port->PIO_PDR |= g_APinDescription[pin].ulPin;
  port->PIO_IER |= g_APinDescription[pin].ulPin;
  PWM_INTERFACE->PWM_ENA |= channelMask;
}
#define ISRQueueName(Q) Queue##Q##ISR
#define ISRQueueNameStr(Q) "Queue" #Q "ISR"

#define DUE_STEPPER_ISR(Q)                                                     \
  void Queue##Q##ISR() {                                                       \
    /*These need only be looked up once rather than multiple times in the      \
     * isr*/                                                                   \
    /*One would hope the compiler would do this, but how is it supposed to*/   \
    /*know if these values change or not?  const requires setting it at */     \
    /*instantiation or casting...this is the next best thing :) */             \
    static StepperQueue* const q = &fas_queue[Q];                              \
    const PWMCHANNELMAPPING* mapping =                                         \
        (const PWMCHANNELMAPPING*)fas_queue[Q].driver_data;                    \
    static uint32_t channel = mapping->channel;                                \
    static uint32_t samPin = g_APinDescription[mapping->pin].ulPin;            \
    static Pio* port = mapping->port;                                          \
    if (!q->_hasISRactive || q->_pauseCommanded) {                             \
      /*In the immortal words of Richard Gray aka levelord, */                 \
      /*"YOU'RE NOT SUPPOSED TO BE HERE!" */                                   \
      return;                                                                  \
    }                                                                          \
    IncrementQueue(Q);                                                         \
    uint8_t rp = q->read_idx;                                                  \
    /*This case should hopefully be elimiated....Unfortunately, it is not :(*/ \
    /*It is quite rare though.*/                                               \
    if (rp == q->next_write_idx) {                                             \
      /*Double interrupt - bail before we destroy the read index!*/            \
      return;                                                                  \
    }                                                                          \
    struct queue_entry* e = &q->entry[rp & QUEUE_LEN_MASK];                    \
    uint8_t s = --e->steps;                                                    \
    /*Obvious case, if we have done all of the steps in this queue entry....*/ \
    if (s == 0) {                                                              \
      /*Setup for the next move...The PWM Peripheral is pretty smart.  We set  \
        the period "update" register, and it will take effect at the end of    \
        the current period.  Since we arent changing the clock, there is no    \
        need to change the duty cycle, just the period...                      \
        Check if we need to toggle direction....If so, we will also need to    \
        delay...Does this ever happen?  I think the dirPinBusy function        \
        prevents this...*/                                                     \
      if (e->toggle_dir) {                                                     \
        e->toggle_dir = 0;                                                     \
        *q->_dirPinPort ^= q->_dirPinMask;                                     \
      }                                                                        \
      /*Check immediatly if we need to turn off the pulse generator.Time is    \
        critical! */                                                           \
      rp++;                                                                    \
      if (rp == q->next_write_idx) {                                           \
        delayMicroseconds(10);                                                 \
        disconnectPWMPeriphal(mapping->port, mapping->pin,                     \
                              mapping->channelMask);                           \
        q->_hasISRactive = false;                                              \
        q->_connected = false;                                                 \
        /*Update the queue pointer...setup for the next move.*/                \
        q->read_idx = rp;                                                      \
        return;                                                                \
      }                                                                        \
      /*Since we're done with e, e is now e_next...*/                          \
      e = &q->entry[rp & QUEUE_LEN_MASK];                                      \
      q->read_idx = rp;                                                        \
      /*We need to look for queue entries with some sort of command            \
        in them..*/                                                            \
      if (e->steps == 0 || !e->hasSteps) {                                     \
        delayMicroseconds(7);                                                  \
        q->_pauseCommanded = true;                                             \
        q->timePWMInterruptEnabled = micros();                                 \
        PWM_INTERFACE->PWM_IER1 = mapping->channelMask;                        \
        port->PIO_CODR = samPin;                                               \
        port->PIO_OER = samPin;                                                \
        port->PIO_PER = samPin;                                                \
        port->PIO_IDR = samPin;                                                \
        PWM_INTERFACE->PWM_CH_NUM[channel].PWM_CPRDUPD = e->ticks;             \
        return;                                                                \
      }                                                                        \
      /*That should be it...We should start the next command on the next       \
        cycle */                                                               \
      AddToTotalSteps(Q, e->steps);                                            \
      PWM_INTERFACE->PWM_CH_NUM[channel].PWM_CPRDUPD = e->ticks;               \
      if (e->toggle_dir) {                                                     \
        e->toggle_dir = 0;                                                     \
        *q->_dirPinPort ^= q->_dirPinMask;                                     \
        delayMicroseconds(30);                                                 \
      }                                                                        \
      return;                                                                  \
    }                                                                          \
  }

DUE_STEPPER_ISR(0)
DUE_STEPPER_ISR(1)
DUE_STEPPER_ISR(2)
DUE_STEPPER_ISR(3)
DUE_STEPPER_ISR(4)
DUE_STEPPER_ISR(5)

inline uint32_t pinToChannel(uint32_t pin) {
  switch (pin) {
    case 34:
    case 67:
    case 73:
    case 35:
      return 0;
    case 17:
    case 36:
    case 72:
    case 37:
    case 42:
      return 1;
    case 38:
    case 43:
    case 63:
    case 39:
      return 2;
    case 40:
    case 64:
    case 69:
    case 41:
      return 3;
    case 9:
      return 4;
    case 8:
    case 44:
      return 5;
    case 7:
    case 45:
      return 6;
    case 6:
      return 7;
  }
  return 0xFFFFFFFF;
}

void StepperQueue::init(uint8_t queue_num, uint8_t step_pin) {
  _queue_num = queue_num;
  driver_data = (void*)&gChannelMap[queue_num];
  _initVars();
  _step_pin = step_pin;
  channelsUsed[pinToChannel(step_pin)] = true;
  numChannels++;
#if defined(KEEP_SCORE)
  totalPulsesDetected[queue_num] = 0;
  totalSteps[queue_num] = 0;
#endif
  gChannelMap[queue_num].pin = step_pin;
  gChannelMap[queue_num].channel = pinToChannel(step_pin);
  TimerChannel_Map[gChannelMap[queue_num].channel] = queue_num;
  gChannelMap[queue_num].port = g_APinDescription[step_pin].pPort;
  gChannelMap[queue_num].channelMask = 1 << gChannelMap[queue_num].channel;
  static bool isPWMEnabled = false;
  if (!isPWMEnabled) {
    pmc_enable_periph_clk(PWM_INTERFACE_ID);
    // ESP32 operates at 3x the speed as the due, which is reasonable to say the
    // esp32 should be 3x as capable.  In the code for the esp, the comment
    // says the clock is 160/5, but the prescaler is set to 5-1? so its at
    // 40MHz. We'll pick a slower clock rate for for this 255 for the prescaler,
    // and a mod clock of 4 for 84,000,000/4=21,000,000/255 is 82352.9....52
    // might be the better number...This puts us at roughly 1/2 the performance
    // of the esp32...Will have to see how many channels it can tolerate...
    // remain compatible with due - We'll use MCK or clk B as necessary.
    PWMC_ConfigureClocks(PWM_FREQUENCY * PWM_MAX_DUTY_CYCLE, VARIANT_MCK / 4,
                         VARIANT_MCK);
    isPWMEnabled = true;
  }
  static bool isTCEnabled = false;
  if (!isTCEnabled) {
    // Lets set it up on a 2ms timer.  That should keep the queue full...
    // Enable the peripheral
    // gin66: Better to remove any Serial.println() cause an application may
    // choose to not use Serial
    // Yes, agreed.  I had meant to do that a long time ago!
    //  Serial.println("Enabling Timer Channel 5");
    pmc_enable_periph_clk(ID_TC5);
    TC1->TC_CHANNEL[2].TC_CCR = 1;
    TC1->TC_CHANNEL[2].TC_CMR = 0;
    TC1->TC_CHANNEL[2].TC_CMR = 0x80 | 0x40;  // Up counter reset on C
                                              // comparison
    // We want 2ms, we're using MCK/2 - 42MHz, so we need a count of 84000
    // Set A B and C registers so we guarantee a hit :)
    TC1->TC_CHANNEL[2].TC_RA = 84000;
    TC1->TC_CHANNEL[2].TC_RB = 84000;
    TC1->TC_CHANNEL[2].TC_RC = 84000;
    TC1->TC_CHANNEL[2].TC_IER = 0;
    TC1->TC_CHANNEL[2].TC_IER = TC_IER_CPCS;
    TC1->TC_CHANNEL[2].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
    NVIC_SetPriority(TC5_IRQn, 1);
    NVIC_EnableIRQ(TC5_IRQn);
  }

  digitalWrite(step_pin, LOW);
  pinMode(step_pin, OUTPUT);
  NVIC_SetPriority(PIOA_IRQn, 0);
  NVIC_SetPriority(PIOB_IRQn, 0);
  NVIC_SetPriority(PIOC_IRQn, 0);
  NVIC_SetPriority(PIOD_IRQn, 0);
  PWM_INTERFACE->PWM_IDR2 = 0x00FFFF0F;
  PWM_INTERFACE->PWM_IDR1 = 0x00FFFF0F;
  NVIC_EnableIRQ(PWM_IRQn);
  PWM_INTERFACE->PWM_IDR2 = 0x00FFFF0F;
  PWM_INTERFACE->PWM_IDR1 = 0x00FFFF0F;

  connect();
}

void StepperQueue::connect() {
  PIO_Configure(g_APinDescription[_step_pin].pPort,
                g_APinDescription[_step_pin].ulPinType,
                g_APinDescription[_step_pin].ulPin,
                g_APinDescription[_step_pin].ulPinConfiguration);
  const PWMCHANNELMAPPING* mapping = (const PWMCHANNELMAPPING*)driver_data;
  PWMC_ConfigureChannel(PWM_INTERFACE, mapping->channel, PWM_CMR_CPRE_CLKB, 0,
                        0);
  // 21 pulses makes for a microsecond pulse.  I believe my drivers need 5us.
  // I'll set this to 10 just to make sure the drivers receive this.  At 20us
  // pulse speed, this is still a 1/2 duty cycle PWM.  Should be easy for any
  // driver to pickup.

  PWM_INTERFACE->PWM_IDR2 = 0x00FFFF0F;
  PWM_INTERFACE->PWM_IDR1 = 0x00FFFF0F;
  PWM_INTERFACE->PWM_DIS = PWM_INTERFACE->PWM_DIS & (~mapping->channelMask);
  PWMC_SetPeriod(PWM_INTERFACE, mapping->channel, 65535);
  PWM_INTERFACE->PWM_DIS = PWM_INTERFACE->PWM_DIS & (~mapping->channelMask);
  PWM_INTERFACE->PWM_CH_NUM[mapping->channel].PWM_CDTY = 21 * 10;
  PWM_INTERFACE->PWM_DIS = PWM_INTERFACE->PWM_DIS & (~mapping->channelMask);
  PWM_INTERFACE->PWM_IDR2 = 0x00FFFF0F;
  PWM_INTERFACE->PWM_IDR1 = 0x00FFFF0F;

  // Rising edge, so we get the interrupt at the beginning of the pulse (gives
  // us some extra time) and so we only get 1 int/pulse Change would give 2.
  switch (_queue_num) {
    case 0:
      attachInterrupt(digitalPinToInterrupt(_step_pin), ISRQueueName(0),
                      RISING);
      break;
    case 1:
      attachInterrupt(digitalPinToInterrupt(_step_pin), ISRQueueName(1),
                      RISING);
      break;
    case 2:
      attachInterrupt(digitalPinToInterrupt(_step_pin), ISRQueueName(2),
                      RISING);
      break;
    case 3:
      attachInterrupt(digitalPinToInterrupt(_step_pin), ISRQueueName(3),
                      RISING);
      break;
    case 4:
      attachInterrupt(digitalPinToInterrupt(_step_pin), ISRQueueName(4),
                      RISING);
      break;
    case 5:
      attachInterrupt(digitalPinToInterrupt(_step_pin), ISRQueueName(5),
                      RISING);
      break;
  }
  _connected = true;
  PWM_INTERFACE->PWM_IDR2 = 0x00FFFF0F;
  PWM_INTERFACE->PWM_IDR1 = 0x00FFFF0F;
}

void StepperQueue::disconnect() {
  const PWMCHANNELMAPPING* mapping = (const PWMCHANNELMAPPING*)driver_data;
  PWMC_DisableChannel(PWM_INTERFACE, mapping->channel);
  PWM_INTERFACE->PWM_DIS = PWM_INTERFACE->PWM_DIS & (~mapping->channelMask);

  // gin66: Shouldn't there be a detachInterrupt() in order to not have stray
  // interrupt ?
  // disconnect is a strange term for what we're doing.  Instead of
  // disconnecting interrupts, we disable the source of the interrupts.  If
  // something external causes a signal on the pin, yes, we will get an
  // interrupt.  That should never happen...but there is code in the interrupt
  // to detect it because I used that very issue to debug the delay code :)  If
  // I got a pulse while in delay mode, I knew there was a problem.  I had it
  // strobe a pin I could see on the logic analyzer.  It made it very easy to
  // see when "wrong" behavior was happening.
  _connected = false;
}

// gin66: I have reworked all code to only use startQueue().
//        This appears to be less complicated.
// I reworked the rework...sorry, it was easier to start with something that
// worked and then remove unnecessary code as there was clearly unnecessary
// code :)
void StepperQueue::startQueue() {
  // This is called only, if isRunning() == false
  noInterrupts();
  struct queue_entry* e = &entry[read_idx & QUEUE_LEN_MASK];
  // Somewhere there must be a call to nointerrupts, because addinf this solved
  // the no-start issue.
  interrupts();
  _hasISRactive = true;

  const PWMCHANNELMAPPING* mapping = (const PWMCHANNELMAPPING*)driver_data;

  // I had this reversed, but the situation where we are starting, but the
  // PWM peripheral is running already doesn't come up often.  If the
  // channel is enabled, we need to use the UPD register to trigger the
  // update on the next cycle, otherwise we can directly set PWM_CPRD.
  // Setting the UPD register instead of the direct register fortunately wasn't
  // harmful, and works as expected.  I'd still leave this check in here just
  // in case, though I believe everything is controlled well enough to not need
  // it.

  if (PWM_INTERFACE->PWM_SR & (1 << mapping->channel)) {
    PWM_INTERFACE->PWM_CH_NUM[mapping->channel].PWM_CPRDUPD = e->ticks;
  } else {
    PWM_INTERFACE->PWM_CH_NUM[mapping->channel].PWM_CPRD = e->ticks;
  }

  if (e->steps > 0 || e->hasSteps) {
    attachPWMPeripheral(mapping->port, mapping->pin, mapping->channelMask);
    return;
  } else {
    // I could see the confusion...man was I tired of this code when I
    // committed :)  It was quite dirty.  This case is when we DO NOT
    // want to fully attach the PWM peripheral, in that we do not want it
    // outputting anything.  So we set the period, but do not attach it.  You
    // moved the period to the top, and I changed it to be the period change
    // with the check to see if we were already running, which is unlikely
    // here, but better to be certain.  The rest of this function is about
    // making certain the output is disabled, but the PWM interrupt used for
    // delays only is enabled and working.
    //
    // I had found that every single move command always started with a delay.
    // So even in startQueue, this is necessary.  I had seen that even before
    // the change for adding delays for the dir pin!  So this block is still
    // entirely necessary :)  I'll make better comments though in the code
    // below...

    // Disconnect the step pin entirely, and force its output to 0!  We're
    // in a delay, we do not want extra pulses generated!
    PIO_SetOutput(mapping->port, g_APinDescription[mapping->pin].ulPin, 0, 0,
                  0);
    // Disable the PIO interrupt.  We probably do not need to do this, and I
    // tested my test code without it, but it is still probably the "right
    // thing" to do.  Don't jump into an interrupt unless we specifically
    // say its ok to do so! :)
    mapping->port->PIO_IDR = g_APinDescription[mapping->pin].ulPin;
    // Here we enable the PWM peripheral, despite having its output
    // disconnected.  This is how we manage the delays.  The pwm peripheral
    // happily counts the time until its supposed to generate a pulse.  It
    // generates the pulse into a high impedance output, because the PIO
    // module has it switched off, but it still generates a PWM interrupt
    // for us, so we know to advance the queue using the PWM ISR.
    PWM_INTERFACE->PWM_ENA |= mapping->channelMask;
    _pauseCommanded = true;
    // We do not want to set timePWMInterruptEnabled here.  It takes a bit to
    // explain, but the purpose of the timePWMInterruptEnabled is to check to
    // see if we are in the PWM interrupt for the same interrupt as the pulse.
    // That seems strange and it is.  I could probably have used the PWM
    // interrupts only for this purpose, but it was actually nice to seperate
    // the delay ISR from the pulse ISR.  It made things clean...up until it
    // didn't.  We would enable the PWM ISR and it would immediatly trigger
    // from the PIO ISR.  The only reliable way to deal with it was to see if
    // we were within a hundred microseconds of the pulse.  If we were, we
    // skipped that ISR call.  Without it, we'd end up skipping the delay.
    // Enabling the check here, well, we don't seem to get to the ISR within
    // 100 microseconds anyhow, so it...shouldn't....cause any issues if
    // present, but why tempt fate/Murphy?  We never want to skip a PWM
    // interrupt at this point, so lets not give it the opportunity :)

    /*Enable the ISR too so we don't miss it*/
    PWM_INTERFACE->PWM_IER1 = PWM_INTERFACE->PWM_IMR1 | mapping->channelMask;
    return;
  }
}

void StepperQueue::forceStop() {
  noInterrupts();
  read_idx = next_write_idx;
  interrupts();
  const PWMCHANNELMAPPING* mapping = (const PWMCHANNELMAPPING*)driver_data;
  PWMC_DisableChannel(PWM_INTERFACE, mapping->channel);
  // gin66: I have added this line in the hope to make it work
  _hasISRactive = false;
}

bool StepperQueue::isValidStepPin(uint8_t step_pin) {
  if (pinToChannel(step_pin) < 0x08) {
    if (!channelsUsed[pinToChannel(step_pin)]) {
      return true;
    }
  }
  return false;
}

bool StepperQueue::isRunning() { return _hasISRactive; }

int8_t StepperQueue::queueNumForStepPin(uint8_t step_pin) { return -1; }

void StepperQueue::adjustSpeedToStepperCount(uint8_t steppers) {
  max_speed_in_ticks = 420;  // This equals 50kHz @ 21MHz
}

void fas_init_engine(FastAccelStepperEngine* engine, uint8_t cpu_core) {
  fas_engine = engine;
}
#endif
