#include "scanCurrentState.h"
#include "Charger.h"

IState* scanCurrentState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        IState* newState = this;        
        if(cycleNum == 9 || sensor.currentInput > currentLimit){
          cycleNum = 0;
          charger.pwmController.incrementDuty(-SCAN_STEP/2);
          charger.pwmController.initIIR();
          newState = charger.goFloat();
        }else{
          cycleNum++;
          charger.pwmController.incrementDuty(SCAN_STEP);    
        }
        return newState;   
    };
