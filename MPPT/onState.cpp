#include "SensorsData.h"
#include "onState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* onState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
    StateFlow<IState*> flow(this);
    charger.mpptReached = 0;
    int floatV = charger.floatVoltageTempCorrectedRaw(sensor);    
    flow
      .thenIf([&] { bool reverseCurrentDetected = sensor.rawCurrentIn <= 0; return reverseCurrentDetected; },
            [&] {
              return charger.goOff(currentTime);
            }
      )
      .doIf([&] { return isRescaningPV; },
        [&] {
          charger.pwmController.resume();
          isRescaningPV = false;
        }
      )
      .doIf([&] { bool shouldRescan = currentTime - lastRescanTime > RESCAN_INTERVAL; return shouldRescan; },
        [&] {
          lastRescanTime = currentTime;
          charger.pwmController.shutdown();  
          isRescaningPV = true;
        }
       )
      .thenIf([&] { bool shouldGoFloat = (sensor.rawBatteryV > floatV) && (sensor.PVvoltage > sensor.batteryV); return shouldGoFloat; },
        [&] {
          return charger.goFloat();                               // battery float voltage go to the charger battery float state
        }
      )
      .thenIf([&] { bool shouldGoBulk = charger.sol_watts > LOW_SOL_WATTS; return shouldGoBulk;},
        [&] {              
          // else if we are making more power than low solar watts figure out what the pwm
          charger.pwmController.setMinDuty();
          charger.pwmController.initIIR();
          return charger.goScan(false);
        }
      );
      return flow.get();
    }   
