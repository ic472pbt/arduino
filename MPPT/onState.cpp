#include "SensorsData.h"
#include "onState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* onState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
 {
    StateFlow<IState*> flow(this);
    flow
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
      .thenIf([&] { bool shouldGoOff = sensor.rawCurrentIn <= 0; return shouldGoOff; },
        [&] {
          charger.minPVVoltage = sensor.PVvoltageFloat; // store latest PV voltage to transit to the onState later
          charger.batteryAtFullCapacity = false;
          return charger.goOff(currentTime);
        }
      )
      .thenIf([&] { 
          int floatVoltage = charger.floatVoltageTempCorrectedRaw(sensor);    
          bool shouldGoFloat = charger.finishAbsorbing && (sensor.rawBatteryV > floatVoltage) && (sensor.PVvoltage > sensor.batteryV); 
          return shouldGoFloat; 
        },
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
