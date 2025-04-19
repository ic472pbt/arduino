#ifndef BULKSTATE_H
#define BULKSTATE_H
#define MAX_PWM_DELTA 4
#define RESCAN_INTERVAL 300000U // 5 min

#include "IState.h"
class Charger;  // Forward declaration of Charger

//  Bulk State - this is charger state for solar watts > MIN_SOL_WATTS. This is where we do the bulk of the battery
//      charging and where we run the Peak Power Tracking alogorithm. In this state we try and run the maximum amount
//      of current that the solar panels are generating into the battery.
class bulkState : public IState {
  private:
    unsigned long 
      lastTrackingTime,
      rawPowerPrev   = 0;
    byte 
      stepSize = MAX_PWM_DELTA;
    int 
      delta = 10,      
      voltageInputPrev = 0;
  public:
    unsigned long 
      lastRescanTime;

    bulkState() : IState("bulk") {}
    
    IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) override;
    bool isFloat() override {return false;}
    bool isBulk() override {return true;}
    bool isOff() override {return false;}
};
#endif
