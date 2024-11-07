#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
          IState* newState = this;
          int effectiveBound = sensor.floatVoltageRaw + sensor.tempCompensationRaw - charger.powerCompensation;
          charger.mpptReached = 0;
          if(!charger.finishEqualize && charger.rawBatteryV > sensor.floatVoltageRaw + sensor.tempCompensationRaw - 25){
            if(charger.absorptionStartTime == 0) charger.absorptionStartTime = currentTime;
            else {
              long interval = currentTime - charger.absorptionStartTime;
              if(interval > 10000) {          
                charger.absorptionAccTime += interval; 
                charger.absorptionStartTime = currentTime;
              }
            }
          } else charger.absorptionStartTime == 0;
          
          if (sensor.PVvoltage + 0.2 < sensor.batteryV) {        // if watts input from the solar panel is less than
            newState = &charger.offInstance;                     // the minimum solar watts then it is getting dark so
            charger.off_count = OFF_NUM;                                // go to the charger off state  
            charger.pwmController.shutdown();         
          }        
          else if (charger.rawBatteryV > effectiveBound) {                    // If we've charged the battery above the float voltage                   
            int delta = (charger.rawBatteryV - (sensor.floatVoltageRaw + sensor.tempCompensationRaw - charger.powerCompensation)) / 2 
              // + charger.stepsDown * 2
              ; 
            // Serial.print(batteryV);Serial.print("decrease "); Serial.println(delta);
            charger.pwmController.incrementDuty(-delta);                                      // down
          }
          else if (charger.rawBatteryV <= effectiveBound) {                    // else if the battery voltage is less than the float voltage - 0.1
            //int delta = 2; // (batteryV - (floatVoltage + tempCompensation - powerCompensation)) / 0.01;
            // Serial.print(batteryV);Serial.print("increase "); Serial.println(delta);
            if(charger.pwmController.duty < charger.pwmController.mpptDuty){   // protect duty from drifting up
              charger.pwmController.incrementDuty(2);                                              // up
            }
            if (charger.rawBatteryV < sensor.floatVoltageRaw + sensor.tempCompensationRaw - 40){ //(floatVoltage + tempCompensation - 0.6)) {               // if the voltage drops because of added load,
              if(!charger.finishEqualize && charger.absorptionStartTime > 0){
                charger.absorptionAccTime += currentTime - charger.absorptionStartTime;
                charger.absorptionStartTime = 0;
              }
              newState = &charger.scanInstance;        // switch back into bulk state to keep the voltage up
              this->dirrection = 1;
              charger.startTracking = true;
            }
          }
          return newState;
  };
