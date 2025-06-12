// Charger.h
#ifndef CHARGER_H
#define CHARGER_H

#define ABSORPTION_TIME_LIMIT 7200000L  // max 2h of topping up per day
#define RESCAN_INTERVAL 299500U //  PV sensing every 5 min. Switch off PWM controler for this.
#define BATT_FLOAT 13.80            // battery voltage we want to stop charging at
#define BATT_FLOAT_RAW 924 // 617          // raw battery voltage we want to stop charging at

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
      rawSolarV             = 0,           // SYSTEM PARAMETER - 
      tempCompensationRaw   = 0;           // SYSTEM PARAMETER - Voltage offset for ambient temperature
    byte
        mpptReached        = 1,
        stepsDown          = 0;    // number of scan steps to decrease on overpower event
    bool 
      isPVoffline = true,          // indicates no sun or PV disconnection  
      finishAbsorbing = false,
      startTracking = true,  
      batteryAtFullCapacity = false;

    unsigned long
      lastSensorsUpdateTime = 0,
      absorptionStartTime   = 0,           //SYSTEM PARAMETER -
      absorptionAccTime     = 0;           //SYSTEM PARAMETER - total time of absorption
    float 
      // min PV voltage to start charging
      minPVVoltage          = BATT_FLOAT,  
      // Input solar power in Watts
      sol_watts;                     
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
        sensorDelay = (currentState->isFloat() && !batteryAtFullCapacity) ? 5000 : 200;
        if(currentTime - lastSensorsUpdateTime > sensorDelay){ // sensors inertia delay
          lastSensorsUpdateTime = currentTime;

          if(ERR>0){
            currentState = goOff(currentTime);
            return;
          }else if (REC==1){ // wait for recovery from low solar voltage (starting a new day)
            REC=0;
            //pwmController.setMinDuty(); 
            currentState = goFloat();
          }
    
          if(absorptionAccTime >= ABSORPTION_TIME_LIMIT) {
            sensor.floatVoltageRaw = BATT_FLOAT_RAW;  
            finishAbsorbing = true;
          }

          IState* newState = currentState->Handle(*this, sensor, currentTime);
          if (newState != currentState) {          
            currentState = newState; // Update to the new state if there's a transition
          }
        }
    }

    void Reverse() {dirrection *= -1;}  
    int floatVoltageTempCorrectedRaw(SensorsData& sensor) { return BATT_FLOAT_RAW + tempCompensationRaw; }
    int maxVoltageTempCorrectedRaw(SensorsData& sensor) { return MAX_BAT_VOLTS_RAW + tempCompensationRaw; }
    bool isAbsorbing(SensorsData& sensor) { return !finishAbsorbing && sensor.rawBatteryV > floatVoltageTempCorrectedRaw(sensor); }
    bool absorbtionStarted() { return absorptionStartTime > 0; }

    void setMaxFloatCurrent(float currentLimit){
      floatInstance.maxCurrent = currentLimit;
    }
    
    
    float getMaxFloatCurrent(){
      return floatInstance.maxCurrent;
    }

    // check if in pv updating cycle
    bool isUpdatingPV(){
      return scanInstance.PVupdate;
    }

    void beginNewDay(SensorsData& sensor){      
      // allow absorbtion
      absorptionAccTime = 0;       
      absorptionStartTime = 0;
      sensor.floatVoltageRaw = MAX_BAT_VOLTS_RAW; 
      finishAbsorbing = false; 
      isPVoffline = false;
    }

    // transit the charger to the off state
    IState* goOff(unsigned long currentTime){
      mpptReached = 0;
      offInstance.offTime = currentTime;              
      pwmController.shutdown(); 
      isPVoffline = true;
      StoreHarvestingData(currentTime);
      return &offInstance;
    }

    // transit to on state
    IState* goOn(){
      mpptReached = 0;
      pwmController.setDuty(MAX_DUTY);
      return &onInstance;
    }

    // transit to the float state
    IState* goFloat(){
      mpptReached = 0;
      if(pwmController.isShuteddown()) pwmController.resume();
      return &floatInstance;
    }

    // transit to the bulk state
    IState* goBulk(unsigned long currentTime){
      batteryAtFullCapacity = false; // if went bulk, then battery is not full
      bulkInstance.lastRescanTime = currentTime;
      return &bulkInstance;
    }  
    
    // transit to the scan state
    IState* goScan(bool PVupdate){
      scanInstance.PVupdate = PVupdate;
      dirrection = 1;
      return &scanInstance;
    }   

    // transit to the current limit scan state
    IState* goCls(){
      pwmController.setMinDuty();
      scanCInstance.currentLimit = floatInstance.maxCurrent;
      return &scanCInstance;
    }         
};
#endif
