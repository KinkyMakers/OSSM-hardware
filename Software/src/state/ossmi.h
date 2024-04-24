#ifndef SOFTWARE_OSSMI_H
#define SOFTWARE_OSSMI_H
class OSSMI {
  public:
    virtual ~OSSMI() = default;
    virtual bool isStrokeTooShort() = 0;
    virtual Menu getMenuOption() = 0;
};
#endif  // SOFTWARE_OSSMI_H
