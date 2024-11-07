#include "scanState.h"
#include "Charger.h"

IState* scanState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        IState* newState = this;        
        if(cycleNum == 10){
          cycleNum = 0;
          bestPower = 0;
          charger.pwmController.setDuty(bestDuty);
          newState = &charger.bulkInstance;
          charger.startTracking = true;
        }else{
          cycleNum++;
          unsigned long solarPower = (unsigned long)charger.rawCurrentIn * sensor.rawBatteryV;
          if(solarPower > bestPower){
            bestPower = solarPower;
            bestDuty = charger.pwmController.duty;
          }
          charger.pwmController.incrementDuty(SCAN_STEP);    
        }
        return newState;   
    };
