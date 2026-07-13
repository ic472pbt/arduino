#include "SensorsData.h"
#include "floatState.h"
#include "Charger.h"
#include "StateFlow.h"
constexpr auto MPPT_FLOAT_TEST_PERIOD_MS = 30000UL;
constexpr auto MPPT_FLOAT_TEST_WINDOW_MS = 200UL;

IState* floatState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) {
    int floatVoltageUpperLimit = min(sensor.maxVoltageRaw, charger.voltageTempCompensateRaw(sensor.floatVoltageRaw));
    int floatVoltageLowerLimit = sensor.getFloatLowLimitRaw();
    int maxVoltageLimit = charger.maxVoltageTempCorrectedRaw(sensor);

    int v = sensor.getRawBatteryV();
    if (prevV == 0) {
      prevV = v;
      prevDv = 0;
    }

    int dv = v - prevV;
    int da = dv - prevDv;

    prevV = v;
    prevDv = dv;

  StateFlow<IState*> flow(this);

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
    .thenIf([&] { return v < floatVoltageLowerLimit; },
      [&]{
          return charger.goScan(false);
    })
    // Emergency brake: hard overvoltage near maxVoltage limit
    .doIf([&] { return v > maxVoltageLimit - 20; },
      [&] {
        int overError = v - (maxVoltageLimit - 20);
        int delta = 15 + overError / 2;
        charger.pwmController.incrementDuty(-min(delta, 30));
    })
    // Predictive regulation at/near float
    .doIf([&] {
        int distance = floatVoltageUpperLimit - v;
        int predictedRise = dv + da * 2;
        
        bool overLimit = distance <= 0;
        bool approachingFast = distance > 0 
            && distance < 50
            && !isTestingDuty
            && (currentTime % MPPT_FLOAT_TEST_PERIOD_MS) >= MPPT_FLOAT_TEST_WINDOW_MS
            && predictedRise > distance / 3;  // relaxed threshold for smoother approach
        
        return overLimit || approachingFast;
      },
      [&] {
        int distance = floatVoltageUpperLimit - v;
        int predictedRise = dv + da * 2;
        int delta = 0;

        if (distance <= 0)
        {
            // above float: proportional response with velocity component
            int overError = -distance;
            delta = 2 + overError / 3 + max(0, dv) / 2;
            delta = min(delta, 15);
        }
        else if (predictedRise > distance)
        {
            delta = 8;
        }
        else if (predictedRise > distance / 2)
        {
            delta = 4;
        }
        else if (predictedRise > distance / 3)
        {
            delta = 2;
        }

        if (delta > 0) {
            charger.pwmController.incrementDuty(-delta);
        }
    })
    // Ramp up toward MPPT when safely below float
    .doIf([&] { return v < floatVoltageUpperLimit && (charger.pwmController.duty < charger.pwmController.mpptDuty); },
      [&] {
        charger.pwmController.incrementDuty(1);
    })
    // Periodic MPPT tracking test in float
    .doIf([&] {
        return v < floatVoltageUpperLimit
            && (charger.pwmController.duty >= charger.pwmController.mpptDuty)
            && !isTestingDuty
            && (currentTime % MPPT_FLOAT_TEST_PERIOD_MS) < MPPT_FLOAT_TEST_WINDOW_MS;
      },
      [&] {
        rawPowerPrev = sensor.getRawPower();
        charger.pwmController.incrementDuty(currentTime % 2 == 0 ? 2 : -2);
        isTestingDuty = !isTestingDuty;
    });

  return flow.get();
}