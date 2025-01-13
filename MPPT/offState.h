#ifndef OFFSTATE_H
#define OFFSTATE_H
#define LVD 10.8
#define OFF_MIN_INTERVAL 15000U // 0.25 min

#include "IState.h"
class Charger;  // Forward declaration of Charger

//  Off State - This is state that the charger enters when solar watts < MIN_SOL_WATTS. The charger goes into this
//      off in this state so that power from the battery doesn't leak back into the solar panel. 
class offState : public IState {
  public:
      unsigned long 
        offTime;
      offState() : IState("off") {}
    
    IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) override ;
    bool isFloat() override {return true;}
    bool isBulk() override {return false;}
    bool isOff() override {return true;}
};
#endif
