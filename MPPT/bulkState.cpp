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
    .doIf([&] {bool absorbtionStarted = charger.isAbsorbing(sensor) && !charger.absorbtionStarted(); return absorbtionStarted; },
      [&] {
        charger.absorptionStartTime = currentTime + 1;
        batteryVprevRaw = sensor.getRawBatteryV();
      }
    )
    .doIf([&] {return charger.isAbsorbing(sensor);},
      [&]{
        long interval = currentTime - charger.absorptionStartTime;
        if (interval > 10000) {
          charger.absorptionAccTime += interval;
          charger.absorptionStartTime = currentTime;
        }
        bool overVoltage = sensor.getRawBatteryV() > charger.maxVoltageTempCorrectedRaw(sensor);
        if(sensor.getRawBatteryV() > batteryVprevRaw || overVoltage){ // voltage is growing - slow this down
          batteryVprevRaw = sensor.getRawBatteryV() - (overVoltage ? 20 : 0);
          charger.pwmController.incrementDuty(-(overVoltage ? 10 : 1));
        }else if((sensor.getRawBatteryV() < batteryVprevRaw - 30) && (charger.pwmController.duty < charger.pwmController.mpptDuty)) charger.pwmController.incrementDuty(1);
      }
    )
    .thenIf([&] { 
        int floatV = charger.floatVoltageTempCorrectedRaw(sensor);
        bool shouldGoFloat = sensor.getRawBatteryV() > floatV && charger.isAbsorbingDisabled(); 
        return shouldGoFloat; },
      [&]{
        return charger.goFloat();              // battery float voltage go to the charger battery float state            
      }                          
    )
    .thenIf([&] { bool shouldGoOn = (charger.mpptReached == 1) && (charger.sol_watts < LOW_SOL_WATTS); return shouldGoOn; }, 
      [&]{
          return charger.goOn();                            
      }
    )
    // this is where we do the Peak Power Tracking ro Maximum Power Point algorithm
    .doIf([&]{ bool mpptPoint = (charger.mpptReached == 1) || charger.startTracking; return mpptPoint; },
      [&]{
        if(currentTime - lastTrackingTime > 29900 || charger.startTracking){
          // do perturbation
          rawPowerPrev = sensor.getRawPower();   
          voltageInputPrev = charger.rawSolarV;         
          stepSize = MAX_PWM_DELTA;
          delta = charger.dirrection * stepSize;
          charger.pwmController.initIIR();
          charger.pwmController.incrementDuty(delta);
          lastTrackingTime = currentTime;
          charger.mpptReached = 0; // ! reset MPPT
          charger.startTracking = false;
        }
      }
    )
    .doIf([&] { bool powerIsRising = sensor.getRawPower() > rawPowerPrev; return powerIsRising; },
      [&]{
        rawPowerPrev = sensor.getRawPower();
        charger.pwmController.incrementDuty(delta);
      }
    )
    .doIf([&] { bool powerIsNotRising = sensor.getRawPower() <= rawPowerPrev; return powerIsNotRising; },
      [&] {
        delta = -delta;
        charger.pwmController.incrementDuty(delta);
        delta /= 2;
        if(delta == 0){                                                // MPP Reached                                         
          charger.pwmController.smoothDuty();                          // smooth duty value a bit
          charger.Reverse();                                           // Change the direction for the next tracking.
          charger.pwmController.storeMpptDuty();
          charger.mpptReached = 1;                                     // indicate MPP reached
        } 
      }
    );
    return flow.get();      
}
