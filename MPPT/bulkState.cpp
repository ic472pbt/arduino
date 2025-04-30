#include "SensorsData.h"
#include "bulkState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* bulkState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{     
  StateFlow<IState*> flow(this);

  flow
    .thenIf([&] { bool shouldProbePVOrRescan = currentTime - lastRescanTime > RESCAN_INTERVAL || charger.pwmController.duty > 818; return shouldProbePVOrRescan;},
      [&] {
        lastRescanTime = currentTime;
        return charger.goScan(charger.pwmController.duty <= 818);
      }
    )
    .doIf([&] {bool absorbtionStarted = charger.isAbsorbing(sensor)  && !charger.absorbtionStarted(); return absorbtionStarted; },
      [&] {
        charger.absorptionStartTime = currentTime + 1;
        batteryVprevRaw = sensor.rawBatteryV;
        charger.pwmController.storeMpptDuty();
      }
    )
    .doIf([&] {bool isAbsorbing = charger.isAbsorbing(sensor); return isAbsorbing; },
      [&]{
        long interval = currentTime - charger.absorptionStartTime;
        if (interval > 10000) {
          charger.absorptionAccTime += interval;
          charger.absorptionStartTime = currentTime;
        }
        bool overVoltage = sensor.rawBatteryV > charger.maxVoltageTempCorrectedRaw(sensor);
        if(sensor.rawBatteryV > batteryVprevRaw || overVoltage){ // voltage is growing - slow this down
          batteryVprevRaw = sensor.rawBatteryV - (overVoltage ? 20 : 0);
          charger.pwmController.incrementDuty(-(overVoltage ? 10 : 2));
        }    
      }
    )
    .thenIf([&] { 
        int floatV = charger.floatVoltageTempCorrectedRaw(sensor);
        bool shouldGoFloat = sensor.rawBatteryV > floatV && charger.finishAbsorbing; 
        return shouldGoFloat; },
      [&]{
        charger.pwmController.storeMpptDuty();
        return charger.goFloat();              // battery float voltage go to the charger battery float state            
      }                          
    )
    // this is where we do the Peak Power Tracking ro Maximum Power Point algorithm
    .thenIf([&]{ bool mpptPoint = (charger.mpptReached == 1) || charger.startTracking; return mpptPoint; },
      [&]{
        if(currentTime - lastTrackingTime > 29900 || charger.startTracking){
          // do perturbation
          rawPowerPrev = sensor.rawPower;   
          voltageInputPrev = charger.rawSolarV;         
          stepSize = MAX_PWM_DELTA;
          delta = charger.dirrection * stepSize;
          charger.pwmController.initIIR();
          charger.pwmController.incrementDuty(delta);
          lastTrackingTime = currentTime;
          charger.mpptReached = 0; // ! reset MPPT
          charger.startTracking = false;
        } else if((charger.sol_watts < LOW_SOL_WATTS) && (charger.mpptReached == 1)){        
          return charger.goOn();                            
        } else if (sensor.rawCurrentIn <= 0) {
           return charger.goOff(currentTime);
        }
        return static_cast<IState*>(this);
      }
    )
    .doIf([&] { bool powerIsRising = sensor.rawPower > rawPowerPrev; return powerIsRising; },
      [&]{
        rawPowerPrev = sensor.rawPower;
        charger.pwmController.incrementDuty(delta);
      }
    )
    .doIf([&] { bool powerIsNotRising = sensor.rawPower <= rawPowerPrev; return powerIsNotRising; },
      [&] {
        delta = -delta;
        charger.pwmController.incrementDuty(delta);
        delta /= 2;
        if(delta == 0){                                                // MPP Reached                                         
          charger.pwmController.smoothDuty();                          // smooth duty value a bit
          charger.Reverse();                                           // Change the direction for the next tracking.
          charger.mpptReached = 1;                                     // indicate MPP reached
        } 
      }
    );
    return flow.get();      
}
