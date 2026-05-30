#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"
#include "StateFlow.h"
constexpr auto BATT_LOW_FLOAT_LIMIT_RAW_PER_CELL = 140; // 13.8 = 924

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) {
    int floatVoltageUpperLimit = max(152 * sensor.getCellCount(), charger.voltageTempCompensateRaw(sensor.floatVoltageRaw));
    // no temperature correction for the lower bound to trigger scan mode transition
    int floatVoltageLowerLimit = BATT_LOW_FLOAT_LIMIT_RAW_PER_CELL * sensor.getCellCount();

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
    .doIf([&] { return isTestingDuty; },
      [&] {
        if(sensor.getRawPower() > rawPowerPrev){
          charger.pwmController.storeMpptDuty(); 
        }
        else{
          charger.pwmController.setDuty(charger.pwmController.mpptDuty);
        }
        isTestingDuty = !isTestingDuty;
    })
    .doIf([&] { bool shouldRecoverFromOverCharge = charger.batteryAtFullCapacity && charger.pwmController.isShuteddown(); return shouldRecoverFromOverCharge; },
      [&] {
        charger.pwmController.slowResume();
      }
    )
    .thenIf([&] { bool reverseCurrentDetected = sensor.getRawCurrentIn() <= 0 && !charger.pwmController.isShuteddown(); return reverseCurrentDetected; },
      [&] {
        return charger.goOff(currentTime);
      }
    )
    .thenIf([&] { bool lowPowerInputDetected = charger.sol_watts <= LOW_SOL_WATTS; return lowPowerInputDetected; },
      [&] {
        return charger.goOn();
      }
    )
//    .doIf([&] { bool batteryOvervolageDetected = sensor.getRawBatteryV() > floatVoltageLowerLimit && !decrementEvent; return batteryOvervolageDetected; },
//      [&] {
//        int delta = (sensor.getRawBatteryV() - floatVoltageLowerLimit) + charger.stepsDown * 4;
//        decrementEvent = true;
//        charger.pwmController.incrementDuty(-delta);
//    })
    .doIf([&] { return sensor.getRawBatteryV() > floatVoltageUpperLimit; },
      [&] {
        charger.pwmController.incrementDuty(-1);
    })
    .doIf([&] { return sensor.getRawBatteryV() < floatVoltageUpperLimit && (charger.pwmController.duty < charger.pwmController.mpptDuty); },
      [&] {
        charger.pwmController.incrementDuty(1);
    })
    .doIf([&] { return sensor.getRawBatteryV() < floatVoltageUpperLimit 
        && (charger.pwmController.duty >= charger.pwmController.mpptDuty) 
        && !isTestingDuty
        && (currentTime % 30000UL) < 200; },
      [&] {
        rawPowerPrev = sensor.getRawPower();
        charger.pwmController.incrementDuty(currentTime % 2 == 0 ? 2 : -2);
        isTestingDuty = !isTestingDuty;
    })
    .thenIf([&] { return sensor.getRawBatteryV() < floatVoltageLowerLimit; },
      [&]{
          charger.stepsDown = max(0, charger.stepsDown - 1);
          charger.batteryAtFullCapacity = false;
          return charger.goScan(false);
    });

  return flow.get();
}