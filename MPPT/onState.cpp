#include "SensorsData.h"
#include "onState.h"
#include "Charger.h"

IState* onState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        IState* newState = this;
        charger.mpptReached = 0;
        if (sensor.PVvoltage + 0.2 < sensor.batteryV) {                      // if watts input from the solar panel is less than
          newState = &charger.goOff();                  // the minimum solar watts then 
        }
        else if (sensor.rawBatteryV > (BATT_FLOAT_RAW - 7)) {          // else if the battery voltage has gotten above the float
          newState = &charger.goFloat();                               // battery float voltage go to the charger battery float state
        }
        else {              
          // else if we are making more power than low solar watts figure out what the pwm
          newState = &charger.scanInstance;
          charger.startTracking = true;
          charger.pwmController.setMinDuty();
          charger.pwmController.initIIR();
          // stepSize = 16; flip = 1;
          /* transitionFromOnToBulk = true;
          // value should be and change the charger to bulk state 
          Serial.print(batteryV); Serial.print(' ');
          Serial.println("go bulk");*/
        }
        return newState;
    }   
