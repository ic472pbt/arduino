#ifndef ISTATE_H
#define ISTATE_H

#include "SensorsData.h"  // Include the PWM class
#include "PWM.h"  // Include the PWM class
class Charger;  // Forward declaration of Charger

class IState{
 protected:
    String stateName;    // Protected so derived classes can set it
   
 public: 
  // Constructor that takes a PWM reference and a state name
  IState(const String& name) :  stateName(name) {}

  virtual ~IState() = default;
  virtual IState* Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) = 0; 
  // Get the name of the state
  const String& GetName() const {
    return stateName;
  }
  virtual bool isFloat() = 0;
  virtual bool isBulk() = 0;
  virtual bool isOff() = 0;
};

#endif // ISTATE_H
