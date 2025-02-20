#include "scanState.h"
#include "Charger.h"

IState* scanState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        static IState* newState;
        if(cycleNum == 0){
          charger.pwmController.shutdown();
          cycleNum++;   
          newState = this;
        } else if(cycleNum > 2 && sensor.PVvoltageFloat > sensor.batteryV){
          cycleNum = 0;
          bestDuty = (unsigned int)(sensor.batteryV * 1023.0 / sensor.PVvoltageFloat) + 55; // try to guess best duty offset (pt) = 0.175 (pt) * battery capacity (Ah)
          charger.pwmController.setDuty(bestDuty);
          charger.pwmController.initIIR();
          newState = charger.goBulk(currentTime);
          charger.startTracking = true;
        } else if(cycleNum > 50){  // 10 seconds guard
          cycleNum = 0;
          newState = charger.goOff(currentTime);
        } else {
          cycleNum++;   
          newState = this;       
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
