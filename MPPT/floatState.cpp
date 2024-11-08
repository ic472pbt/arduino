#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
          IState* newState = this;
          int floatV = charger.floatVoltageTempCorrectedRaw(sensor);
          int effectiveBound = floatV - charger.powerCompensation;
          charger.mpptReached = 0;
          if(!charger.finishEqualize && sensor.rawBatteryV > floatV - 25){
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
            newState = &charger.goOff();                     // the minimum solar watts then it is getting dark so
          }        
          else if (sensor.rawBatteryV > effectiveBound) {                    // If we've charged the battery above the float voltage                   
            int delta = (sensor.rawBatteryV - effectiveBound) / 2 + charger.stepsDown * 4; 
            // Serial.print(batteryV);Serial.print("decrease "); Serial.println(delta);
            incrementsCounter = 0;
            charger.pwmController.incrementDuty(-delta);                                      // down
          }
          else if (sensor.rawBatteryV <= effectiveBound) {                    // else if the battery voltage is less than the float voltage - 0.1
            //int delta = 2; // (batteryV - (floatVoltage + tempCompensation - powerCompensation)) / 0.01;
            // Serial.print(batteryV);Serial.print("increase "); Serial.println(delta);
            if(charger.pwmController.duty < charger.pwmController.mpptDuty){   // protect duty from drifting up
              incrementsCounter++;
              charger.pwmController.incrementDuty(2);                                              // up
              // There is less excess power when there are three consecutive increments. Begin stepsDown decrease 
              if(incrementsCounter > 3){
                incrementsCounter = 0;
                charger.stepsDown = max(0, charger.stepsDown - 1);
              }
            }
            if (sensor.rawBatteryV < floatV - 40){ //(floatVoltage + tempCompensation - 0.6)) {               // if the voltage drops because of added load,
              if(!charger.finishEqualize && charger.absorptionStartTime > 0){
                charger.absorptionAccTime += currentTime - charger.absorptionStartTime;
                charger.absorptionStartTime = 0;
              }
              newState = &charger.scanInstance;        // switch back into bulk state to keep the voltage up
              charger.dirrection = 1;
              charger.startTracking = true;
            }
          }
          return newState;
  };
