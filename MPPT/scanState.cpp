#include "scanState.h"
#include "Charger.h"

IState* scanState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        static IState* newState;
        if(cycleNum == 0){
          charger.pwmController.shutdown();
          bestPower = cycleNum;
          cycleNum++;   
          newState = this;
        } else if(cycleNum > 0 && PVupdate){
          cycleNum = 0;
          PVupdate = false;
          charger.pwmController.resume();
          newState = charger.goBulk(currentTime);
        } else if(cycleNum > 20){
          cycleNum = 0;
          // bestDuty = (unsigned int)(sensor.batteryV * 1023.0 / sensor.PVvoltageFloat) + 55; // try to guess best duty offset (pt) = 0.175 (pt) * battery capacity (Ah)
          charger.pwmController.setDuty(bestDuty);
          charger.pwmController.initIIR();
          newState = charger.goBulk(currentTime);
          charger.startTracking = true;
        } else {
          cycleNum++;   
          newState = this;  
          if(sensor.rawPower > bestPower){
            bestPower = sensor.rawPower;
            bestDuty = charger.pwmController.duty;
          }
          charger.pwmController.incrementDuty(SCAN_STEP);   
        }
        return newState;   
    };
