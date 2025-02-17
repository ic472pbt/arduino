// Charger.h
#ifndef CHARGER_H
#define CHARGER_H

#define ABSORPTION_TIME_LIMIT 7200000L  // max 2h of topping up per day

#include "SensorsData.h"
#include "PWM.h"
#include "IState.h"
#include "floatState.h"
#include "scanState.h"
#include "scanCurrentState.h"
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
    scanState scanInstance;
    scanCurrentState scanCInstance;
    bulkState bulkInstance;
    
    unsigned int 
      sensorDelay = 200, 
      period;
public:
    int
      powerCompensation     = 0,      
      rawSolarV             = 0,           // SYSTEM PARAMETER - 
      tempCompensationRaw   = 0;           // SYSTEM PARAMETER - Voltage offset for ambient temperature
    byte
        mpptReached        = 1,
        stepsDown          = 0;    // number of scan steps to decrease on overpower event
    bool 
      finishEqualize = false,
      startTracking = true,  
      powerCapMode = false;

    unsigned long
      lastSensorsUpdateTime = 0,
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
    
    // Constructor that accepts an IIRFilter reference
    Charger(IIRFilter& filter) : pwmController(filter), currentState(&offInstance) {}

    // Additional Charger-specific methods can be added here
    void initializePWM(unsigned long frequency = 40) {
        pwmController.initialize(frequency);  // Initialize PWM with default frequency
    }

    void Charge(SensorsData& sensor, unsigned long currentTime){
        if(currentTime - lastSensorsUpdateTime > sensorDelay){ // sensors inertia delay
          lastSensorsUpdateTime = currentTime;

          if(ERR>0){
            currentState = goOff(currentTime);
            pwmController.incrementDuty(-100);
            pwmController.shutdown();
            return;
          }else if (REC==1){ // wait for recovery from low solar voltage (starting a new day)
            REC=0;
            pwmController.setMinDuty(); 
            currentState = goFloat();
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
            sensorDelay = currentState->isFloat() ? 5000 : 200;
            currentState = newState; // Update to the new state if there's a transition
          }
        }
    }

    void Reverse() {dirrection *= -1;}  
    int floatVoltageTempCorrectedRaw(SensorsData& sensor) { return sensor.floatVoltageRaw + tempCompensationRaw; }

    void setMaxFloatCurrent(float currentLimit){
      floatInstance.maxCurrent = currentLimit;
    }

    float getMaxFloatCurrent(){
      return floatInstance.maxCurrent;
    }

    // transit the charger to the off state
    IState* goOff(unsigned long currentTime){
      offInstance.offTime = currentTime;              
      pwmController.shutdown(); 
      return &offInstance;
    }

    // transit to on state
    IState* goOn(){
      pwmController.setMaxDuty();
      return &onInstance;
    }

    // transit to the float state
    IState* goFloat(){
      pwmController.resume();
      return &floatInstance;
    }

    // transit to the bulk state
    IState* goBulk(unsigned long currentTime){
      bulkInstance.lastRescanTime = currentTime;
      return &bulkInstance;
    }  
    
    // transit to the scan state
    IState* goScan(){
      startTracking = true;
      dirrection = 1;
      pwmController.shutdown();
      return &scanInstance;
    }   

    // transit to the current limit scan state
    IState* goCls(){
      pwmController.setDuty(5);
      scanCInstance.currentLimit = floatInstance.maxCurrent;
      return &scanCInstance;
    }         
};
#endif
