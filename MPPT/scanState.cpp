#include "scanState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* scanState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
  StateFlow<IState*> flow(this);

  flow
    .doIfFlag(cycleNum == 0, 
      [&] {
          charger.pwmController.shutdown();
          bestPower = 0;
          cycleNum++;   
      })
    .thenIf([&] { return cycleNum > 0 && PVupdate; }, 
      [&]{
          cycleNum = 0;
          PVupdate = false;
          charger.pwmController.resume();
          return charger.goBulk(currentTime);
      })
    .thenIf([&] { return sensor.getRawBatteryV() > sensor.floatVoltageLimitRaw; }, 
      [&]{
          charger.pwmController.incrementDuty(-10);   
          return charger.goFloat();
      })
    .thenIf([&] { return cycleNum > 18; },
      [&]{
          cycleNum = 0;
          charger.pwmController.setDuty(bestDuty);
          charger.pwmController.initIIR();
          charger.startTracking = true;
          return charger.goBulk(currentTime);
      })
    .otherwiseDo(
      [&]{
          cycleNum++;   
          if(sensor.getRawPower() > bestPower){
            bestPower = sensor.getRawPower();
            bestDuty = charger.pwmController.duty;
          }
          charger.pwmController.incrementDuty(SCAN_STEP);   
      });
  return flow.get();   
};
