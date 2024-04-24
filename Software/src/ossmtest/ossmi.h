#ifndef SOFTWARE_OSSMI_H
#define SOFTWARE_OSSMI_H
class OSSMI {
  public:
    virtual ~OSSMI() = default;
    virtual void action() = 0;
};
#endif  // SOFTWARE_OSSMI_H
