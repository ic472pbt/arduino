// floatState.h
#ifndef FLOATSTATE_H
#define FLOATSTATE_H
#define CURRENT_ABSOLUTE_MAX 40.0    //  CALIB PARAMETER - Maximum Current The System Can Handle

#include "IState.h"
class Charger;  // Forward declaration of Charger

//  Float State - As the battery charges it's voltage rises. When it gets to the MAX_BAT_VOLTS we are done with the 
//      bulk battery charging and enter the battery float state. In this state we try and keep the battery voltage
//      at MAX_BAT_VOLTS by adjusting the pwm value. If we get to pwm = 100% it means we can't keep the battery 
//      voltage at MAX_BAT_VOLTS which probably means the battery is being drawn down by some load so we need to back
//      into the bulk charging mode.
class floatState : public IState {
private:
  byte
    incrementsCounter = 0;
public: 
    float 
      maxCurrent = CURRENT_ABSOLUTE_MAX;  // max allowed current in float mode    
    floatState() : IState("float") {}
    IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) override ;
    bool isFloat() override {return true;}
    bool isBulk() override {return false;}
    bool isOff() override {return false;}
};
#endif
