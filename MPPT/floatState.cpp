#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"
#include "StateFlow.h"
constexpr auto BATT_LOW_FLOAT_LIMIT_RAW_PER_CELL = 142; // 12.7

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) {
    int floatVoltageUpperLimit = min(sensor.maxVoltageRaw, charger.voltageTempCompensateRaw(sensor.floatVoltageRaw));
    // no temperature correction for the lower bound to trigger scan mode transition
    int floatVoltageLowerLimit = BATT_LOW_FLOAT_LIMIT_RAW_PER_CELL * sensor.getCellCount();

    int v = sensor.getRawBatteryV();
    if (prevV == 0) {
      prevV = v;
      prevDv = 0;
    }

    int dv = v - prevV;
    int da = dv - prevDv;

    prevV = v;
    prevDv = dv;

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
    .thenIf([&] { return sensor.getRawBatteryV() < floatVoltageLowerLimit; },
      [&]{
          return charger.goScan(false);
    })
    .doIf([&] {
        bool overLimit = v >= floatVoltageUpperLimit;
        bool belowLimitNeedsDecrease = v < floatVoltageUpperLimit
            && !isTestingDuty && dv > 0
            && (currentTime % 30000UL) >= 200;
        return overLimit || belowLimitNeedsDecrease;
      },
      [&] {
        int distance = floatVoltageUpperLimit - v;
        int predictedRise = dv + da * 2;
        int delta = 0;

        if (distance <= 0)
        {
            int maxVoltageLimit = charger.maxVoltageTempCorrectedRaw(sensor);
            int overHeadroom = max(1, maxVoltageLimit - floatVoltageUpperLimit);
            int overError = -distance;

            // soft near float, stronger near maxVoltage
            delta = 1 + (overError * 6) / overHeadroom;

            // extra damping if voltage is still rising fast
            if (predictedRise > 0) {
                delta += min(3, predictedRise);
            }

            delta = min(delta, 12);
        }
        else if (predictedRise > distance)
        {
            delta = 10;
        }
        else if (predictedRise > distance / 2)
        {
            delta = 4;
        }

        charger.pwmController.incrementDuty(-delta);
    })
    .doIf([&] { return v < floatVoltageUpperLimit && (charger.pwmController.duty < charger.pwmController.mpptDuty); },
      [&] {
        charger.pwmController.incrementDuty(1);
    })
    .doIf([&] {
        return v < floatVoltageUpperLimit
            && (charger.pwmController.duty >= charger.pwmController.mpptDuty)
            && !isTestingDuty
            && (currentTime % 30000UL) < 200;
      },
      [&] {
        rawPowerPrev = sensor.getRawPower();
        charger.pwmController.incrementDuty(currentTime % 2 == 0 ? 2 : -2);
        isTestingDuty = !isTestingDuty;
    });

  return flow.get();
}