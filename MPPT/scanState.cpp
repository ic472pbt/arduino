#include "scanState.h"
#include "Charger.h"

IState* scanState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        IState* newState = this;        
        if(cycleNum == 1){
          cycleNum = 0;
          bestDuty = (unsigned int)(sensor.batteryV * 1023.0 / sensor.PVvoltage) + 70; // try to guess best duty offset (pt) = 0.175 (pt) * battery capacity (Ah)
          charger.pwmController.setDuty(bestDuty);
          charger.pwmController.initIIR();
          newState = charger.goBulk(currentTime);
          charger.startTracking = true;
        }else{
          cycleNum++;
          charger.pwmController.shutdown();
          /* old algorithm
           *  unsigned long solarPower = (unsigned long)charger.rawCurrentIn * sensor.rawBatteryV;
          if(solarPower > bestPower){
            bestPower = solarPower;
            bestDuty = charger.pwmController.duty;
          }
          charger.pwmController.incrementDuty(SCAN_STEP);    */
        }
        return newState;   
    };
