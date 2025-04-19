#include "SensorsData.h"
#include "onState.h"
#include "Charger.h"

IState* onState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        IState* newState;
        charger.mpptReached = 0;
        int floatV = charger.floatVoltageTempCorrectedRaw(sensor);
        // detect reverse current
        if (sensor.rawCurrentIn <= 0) {                
          newState = charger.goOff(currentTime);                 
        } else if(charger.sol_watts <= LOW_SOL_WATTS){
          newState = this;
        } else if ((sensor.rawBatteryV > floatV) && (sensor.PVvoltage > sensor.batteryV)) {          // else if the battery voltage has gotten above the float
          newState = charger.goFloat();                               // battery float voltage go to the charger battery float state
        } else {              
          // else if we are making more power than low solar watts figure out what the pwm
          newState = charger.goScan(false);
          charger.pwmController.setMinDuty();
          charger.pwmController.initIIR();
        }
        return newState;
    }   
