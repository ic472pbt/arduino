#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) {
  int floatVoltageLimit = charger.floatVoltageTempCorrectedRaw(sensor);
  int effectiveBound = floatVoltageLimit ;

  /* Side effects that should run regardless of transition
  bool isAbsorbing = charger.isAbsorbing();
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
*/
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

        bool drop = sensor.rawBatteryV < floatVoltageLimit - 40;
        bool waiting = isWaitingAfterRecovery && (currentTime - waitingStartTime < MAX_WAITING_AFTER_RECOVERY);

        if (drop && !waiting) {
          isWaitingAfterRecovery = false;
          maxCurrent = CURRENT_ABSOLUTE_MAX;
          charger.stepsDown = max(0, charger.stepsDown - 1);
          return charger.goScan(false);
        }

        return static_cast<IState*>(this);
      }
    );

  return flow.get();
}