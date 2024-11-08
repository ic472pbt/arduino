// Charger.h
#ifndef CHARGER_H
#define CHARGER_H

#define ABSORPTION_TIME_LIMIT 14400000L  // max 4h of topping up per day
#define OFF_NUM 9                   // number of iterations of off charger state

#include "SensorsData.h"
#include "PWM.h"
#include "IState.h"
#include "floatState.h"
#include "scanState.h"
#include "bulkState.h"
#include "onState.h"
#include "offState.h"

extern void StoreHarvestingData(unsigned long);
extern bool REC;
extern byte ERR;

class Charger{
private:
    floatState floatInstance;
    offState offInstance;
    onState onInstance;

public:
    int 
      powerCompensation     = 0,
      rawCurrentIn          = 0,           // SYSTEM PARAMETER - 
      rawSolarV             = 0,           // SYSTEM PARAMETER - 
      tempCompensationRaw   = 0;           // SYSTEM PARAMETER - Voltage offset for ambient temperature
    byte
        off_count = OFF_NUM,
        mpptReached        = 1,
        stepsDown          = 0;    // number of scan steps to decrease on overpower event
    bool 
      finishEqualize = false,
      startTracking = true,  
//      powerCapMode = false,
      controlFloat = false;
    unsigned long
      absorptionStartTime   = 0,           //SYSTEM PARAMETER -
      absorptionAccTime     = 0;           //SYSTEM PARAMETER - total time of absorption
    float 
      sol_watts;                     // SYSTEM PARAMETER - Input power (solar power) in Watts
    char dirrection = 1;  // Public property for accessing flip
    // object variable that holds state for charger state machine
    IState* currentState;
    PWM pwmController;  // PWM instance created within Charger
    // Instances of concrete states
//------------------------------------------------------------------------------------------------------
// This routine is the charger state machine. It has four states on, off, bulk and float.
// It's called once each time through the main loop to see what state the charger should be in.
// The battery charger can be in one of the following five states:
// 
    scanState scanInstance;
    bulkState bulkInstance;
    
    // Constructor that accepts an IIRFilter reference
    Charger(IIRFilter& filter) : pwmController(filter), currentState(&offInstance) {}

    // Additional Charger-specific methods can be added here
    void initializePWM(unsigned long frequency = 40) {
        pwmController.initialize(frequency);  // Initialize PWM with default frequency
    }

    void Charge(SensorsData& sensor, unsigned long currentTime){
        if(ERR>0){
          currentState = &offInstance;
          pwmController.incrementDuty(-100);
          pwmController.shutdown();
          return;
        }else if (REC==1){ // wait for recovery from low solar voltage (starting a new day)
          REC=0;
          pwmController.setMinDuty(); 
          currentState = &floatInstance;
          StoreHarvestingData(currentTime);
          // allow absorbtion
          absorptionAccTime = 0;       
          absorptionStartTime = 0;
          sensor.floatVoltageRaw = MAX_BAT_VOLTS_RAW; 
          finishEqualize = false; 
//          powerCapMode = false;
        }
  
        if(absorptionAccTime >= ABSORPTION_TIME_LIMIT) {
          sensor.floatVoltageRaw = BATT_FLOAT_RAW;  
          finishEqualize = true;
        }

        IState* newState = currentState->Handle(*this, sensor, currentTime);
        if (newState != currentState) {
          currentState = newState; // Update to the new state if there's a transition
        }
    }

    void Reverse() {dirrection *= -1;}  
    int floatVoltageTempCorrectedRaw(SensorsData& sensor) { return sensor.floatVoltageRaw + tempCompensationRaw; }
    
    // transit the charger to the off state
    offState goOff(){
      off_count = OFF_NUM;                  
      pwmController.shutdown(); 
      return offInstance;
    }

    // transit to on state
    onState goOn(){
      pwmController.setMaxDuty();
      return onInstance;
    }

    // transit to the float state
    floatState goFloat(){
      pwmController.storeMpptDuty();
      controlFloat = true;
      pwmController.resume();
      return floatInstance;
    }
};
#endif
