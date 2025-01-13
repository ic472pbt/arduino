#ifndef SCANCURRENTSTATE_H
#define SCANCURRENTSTATE_H
#include "IState.h"
class Charger;  // Forward declaration of Charger

// scanCurrentState - Limit inbound current
class scanCurrentState : public IState {
  private:
    byte cycleNum;

  public:
    float currentLimit = 0.0;
    scanCurrentState() : IState("cls"), cycleNum(0) {}
        
    IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) override;

    bool isFloat() override {return false;}
    bool isBulk() override {return false;}
    bool isOff() override {return false;}
};
#endif
