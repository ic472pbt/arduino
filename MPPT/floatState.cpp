#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) {
  int floatVoltageLimit = charger.floatVoltageTempCorrectedRaw(sensor);
  int effectiveBound = floatVoltageLimit ;
  charger.mpptReached = 0;

  // Side effects that should run regardless of transition
  bool isAbsorbing = !charger.finishEqualize && sensor.rawBatteryV > floatVoltageLimit - 25;
  if (isAbsorbing && charger.absorptionStartTime == 0) {
      charger.absorptionStartTime = currentTime;
  }
  else if(isAbsorbing) {
      long interval = currentTime - charger.absorptionStartTime;
      if (interval > 10000) {
        charger.absorptionAccTime += interval;
        charger.absorptionStartTime = currentTime;
      }    
  } 
  else charger.absorptionStartTime = 0;

  StateFlow<IState*> flow(this);

  // first matching condition will execute
  flow
    .doIf([&] { bool shouldRecoverFromOverCharge = charger.batteryAtFullCapacity && charger.pwmController.isShuteddown(); return shouldRecoverFromOverCharge; },
      [&] {
        charger.pwmController.slowResume();
      }
    )
    .doIf([&] { bool shouldIncreaseCurrent = charger.batteryAtFullCapacity && sensor.currentInput < maxCurrent; return shouldIncreaseCurrent; },
      [&] {        
        charger.pwmController.incrementDuty(10);
      }
    )
    .doIf([&] { bool targetCurrentReached = charger.batteryAtFullCapacity; return targetCurrentReached; },
      [&] {
        charger.pwmController.incrementDuty(-5);
        isWaitingAfterRecovery = true;
        waitingStartTime = currentTime;
         // go to normal voltage stabilization  
        charger.batteryAtFullCapacity = false;
      }
    )
    .thenIf([&] { bool reverseCurrentDetected = sensor.rawCurrentIn <= 0; return reverseCurrentDetected; },
      [&] {
        isWaitingAfterRecovery = false;
        return charger.goOff(currentTime);
      }
    )
    .thenIf([&] { bool lowPowerInputDetected = charger.sol_watts <= LOW_SOL_WATTS; return lowPowerInputDetected; },
      [&] {
        isWaitingAfterRecovery = false;
        return charger.goOn();
      }
    )
    .doIf([&] { bool batteryOvervolageDetected = sensor.rawBatteryV > effectiveBound; return batteryOvervolageDetected; },
      [&] {
        int delta = (sensor.rawBatteryV - effectiveBound) + charger.stepsDown * 4;
        decrementEvent = true;
        charger.pwmController.incrementDuty(-delta);
      }
    )
    .otherwise(
      [&] {
        if (decrementEvent) {
          decrementEvent = false;
          prevVoltage = sensor.rawBatteryV;
        } else if ((prevVoltage - sensor.rawBatteryV > 0) &&
                   (charger.pwmController.duty < charger.pwmController.mpptDuty)) {
          prevVoltage = sensor.rawBatteryV;
          charger.pwmController.incrementDuty(2);
        }

        bool drop = sensor.rawBatteryV < floatVoltageLimit - 80;
        bool waiting = isWaitingAfterRecovery && (currentTime - waitingStartTime < MAX_WAITING_AFTER_RECOVERY);

        if (drop && !waiting) {
          isWaitingAfterRecovery = false;
          if (!charger.finishEqualize && charger.absorptionStartTime > 0) {
            charger.absorptionAccTime += currentTime - charger.absorptionStartTime;
            charger.absorptionStartTime = 0;
          }
          maxCurrent = CURRENT_ABSOLUTE_MAX;
          charger.stepsDown = max(0, charger.stepsDown - 1);
          return charger.goScan(false);
        }

        return static_cast<IState*>(this);
      }
    );

  return flow.get();
}
/*IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
          IState* newState = this;
          int floatVoltageLimit = charger.floatVoltageTempCorrectedRaw(sensor);
          int effectiveBound = floatVoltageLimit; // - charger.powerCompensation;
          charger.mpptReached = 0;

          if(!charger.finishEqualize && sensor.rawBatteryV > floatVoltageLimit - 25){
            if(charger.absorptionStartTime == 0) charger.absorptionStartTime = currentTime;
            else {
              long interval = currentTime - charger.absorptionStartTime;
              if(interval > 10000) {          
                charger.absorptionAccTime += interval; 
                charger.absorptionStartTime = currentTime;
              }
            }
          } else charger.absorptionStartTime == 0;
          
          if(charger.batteryAtFullCapacity){
            // off state recovery after overcharge
            if(charger.pwmController.isShuteddown()){
              charger.pwmController.slowResume();
            } else if(sensor.currentInput < maxCurrent){
              charger.pwmController.incrementDuty(10);      // current stabilization
            } else{
              charger.pwmController.incrementDuty(-5);
              isWaitingAfterRecovery = true;
              waitingStartTime = currentTime;
              charger.batteryAtFullCapacity = false;        // go to normal voltage stabilization      
            }
          } else if (sensor.rawCurrentIn <= 0) {     // detect reverse current
            isWaitingAfterRecovery = false;
            newState = charger.goOff(currentTime);              
          }else if(charger.sol_watts <= LOW_SOL_WATTS){
            isWaitingAfterRecovery = false;
            newState = charger.goOn();
          }else if (sensor.rawBatteryV > effectiveBound) { // If we've charged the battery above the float voltage
            int delta = (sensor.rawBatteryV - effectiveBound) + charger.stepsDown * 4; 
            // Serial.print(batteryV);Serial.print("decrease "); Serial.println(delta);
            decrementEvent = true;
            charger.pwmController.incrementDuty(-delta);                                      // down
          }else if (sensor.rawBatteryV <= effectiveBound) {                    // else if the battery voltage is less than the float voltage - 0.1
            if(decrementEvent){ // begin voltage monitoring
              decrementEvent = false;
              prevVoltage = sensor.rawBatteryV;
            }else{
              if( (prevVoltage - sensor.rawBatteryV > 0) && (charger.pwmController.duty < charger.pwmController.mpptDuty) ){ // protect duty from excess drifting up
                  prevVoltage = sensor.rawBatteryV;
                  charger.pwmController.incrementDuty(2);   // up
              }
            }
             // if the voltage drops because of added load, and not after recovery to prevent early rescan
            if (sensor.rawBatteryV < floatVoltageLimit - 80 && !(isWaitingAfterRecovery && currentTime-waitingStartTime < MAX_WAITING_AFTER_RECOVERY)){
              isWaitingAfterRecovery = false;
              if(!charger.finishEqualize && charger.absorptionStartTime > 0){
                charger.absorptionAccTime += currentTime - charger.absorptionStartTime;
                charger.absorptionStartTime = 0;
              }
              maxCurrent = CURRENT_ABSOLUTE_MAX;
              // There is less excess power when there are consecutive compensated increments. Begin stepsDown decrease 
              charger.stepsDown = max(0, charger.stepsDown - 1);
              newState = charger.goScan(false);        // switch back into bulk state to keep the voltage up
            }
          }
          return newState;
  };
*/