#ifndef ONSTATE_H
#define ONSTATE_H
constexpr auto LOW_SOL_WATTS = 4.00;    // low bulk mode limit

#include "IState.h"
class Charger;  // Forward declaration of Charger

class onState : public IState {
  private:
      unsigned long 
        lastRescanTime;
      bool
        rescanningPvFlag = false;
  public:
    onState() : IState("on") {}
    
    IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) override;

    bool isFloat() override {return false;}
    bool isBulk() override {return false;}
    bool isOff() override {return false;}
};
#endif
