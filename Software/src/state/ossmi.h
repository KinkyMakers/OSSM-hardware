#ifndef SOFTWARE_OSSMI_H
#define SOFTWARE_OSSMI_H
class OSSMI {
  public:
    virtual ~OSSMI() = default;

    virtual bool isStrokeTooShort() = 0;

    // GETTERS
    virtual Menu getMenuOption() = 0;


    virtual void startHoming() = 0;
    virtual void clearHoming() = 0;
    virtual void reverseHoming() = 0;
};
#endif  // SOFTWARE_OSSMI_H
