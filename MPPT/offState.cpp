#include "SensorsData.h"
#include "offState.h"
#include "Charger.h"
#include "StateFlow.h"

IState* offState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
      StateFlow<IState*> flow(this);
       
      bool canTryToTransit = (currentTime - offTime > OFF_MIN_INTERVAL) && (sensor.PVvoltage > charger.minPVVoltage);
      int floatV = charger.floatVoltageTempCorrectedRaw(sensor);

      flow
        // new charging cycle begin
        .doIf([&] { return canTryToTransit && charger.isPVoffline; },
          [&] {
            charger.isPVoffline = false;
            charger.beginNewDay(sensor);
          }
        )
        .thenIf(
          [&] {
            bool canTransitToScan =  !charger.isPVoffline && (sensor.batteryV > LVD) && (sensor.rawBatteryV < floatV);
            return canTryToTransit && canTransitToScan; 
          }, 
          [&] { return charger.goScan(false);}
        )
        .thenIf(
          [&] { 
            bool canTransitTofloat = charger.batteryAtFullCapacity || (sensor.rawBatteryV > floatV);
            return canTryToTransit && canTransitTofloat;
          },
          [&] { return charger.goFloat(); }
        );

      return flow.get();
    }
