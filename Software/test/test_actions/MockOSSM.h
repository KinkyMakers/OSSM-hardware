#ifndef SOFTWARE_MOCKOSSM_H
#define SOFTWARE_MOCKOSSM_H

#ifndef MOCK_OSSM_H
#define MOCK_OSSM_H

class OSSM {
  public:
    int setupCalled = 0;
    int findHomeCalled = 0;

    void setup() { setupCalled++; }

    void findHome() { findHomeCalled++; }
};

#endif  // MOCK_OSSM_H

#endif  // SOFTWARE_MOCKOSSM_H
