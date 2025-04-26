#ifndef ONSTATE_H
#define ONSTATE_H
#define LOW_SOL_WATTS 5.00         // low bulk mode limit
#define BATT_FLOAT_RAW 924 // 617          // raw battery voltage we want to stop charging at

#include "IState.h"
class Charger;  // Forward declaration of Charger

//  On State - this is charger state for MIN_SOL_WATTS < solar watts < LOW_SOL_WATTS. In this state is the solar 
//      watts input is too low for the bulk charging state but not low enough to go into the off state. 
//      In this state we just set the pwm = 99.9% to get the most of low amount of power available.
class onState : public IState {
  private:
      unsigned long 
        lastRescanTime;
      bool
        isRescaningPV = false;
  public:
    onState() : IState("on") {}
    
    IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) override;

    bool isFloat() override {return false;}
    bool isBulk() override {return false;}
    bool isOff() override {return false;}
};
#endif
