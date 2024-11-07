#include "SensorsData.h"
#include "onState.h"
#include "Charger.h"

IState* onState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
        IState* newState = this;
        charger.mpptReached = 0;
        if (sensor.PVvoltage + 0.2 < sensor.batteryV) {                      // if watts input from the solar panel is less than
          newState = &charger.offInstance;                  // the minimum solar watts then 
          charger.off_count = OFF_NUM;                              // go to the charger off state
          charger.pwmController.shutdown(); 
          /* Serial.print(solarV); Serial.print(' ');
          Serial.print(batteryV);
          Serial.println("go off");*/
        }
        else if (charger.rawBatteryV > (BATT_FLOAT_RAW - 7)) {            // else if the battery voltage has gotten above the float
          newState = &charger.floatInstance;                               // battery float voltage go to the charger battery float state
          /*Serial.print(batteryV); Serial.print(' ');
          Serial.print(BATT_FLOAT);
          Serial.println("go float");*/
        }
        else if (charger.sol_watts < LOW_SOL_WATTS) {                 // else if the solar input watts is less than low solar watts
          charger.pwmController.setMaxDuty();                         // it means there is not much power being generated by the solar panel
                                                              // so we just set the pwm = 100% so we can get as much of this power as possible
          /*Serial.print(batteryV); Serial.print(' ');
          Serial.println("go on");*/
        }                                                     // and stay in the charger on state
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
