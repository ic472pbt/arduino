#include "HardwareSerial.h"
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
        .doIfFlag(canTryToTransit && charger.isPVoffline && !charger.batteryAtFullCapacity,
          [&] {
            //Serial.println("begin new day");
            charger.beginNewDay(sensor);
          }
        )
        .thenIf(
          [&] {
            bool canTransitToScan =  !charger.isPVoffline && (sensor.getBatteryV() > LVD) && (sensor.getRawBatteryV() < floatV);
            return canTryToTransit && canTransitToScan; 
          }, 
          [&] { 
            //Serial.println("transit to scan");
            return charger.goScan(false);
          }
        )
        .thenIf(
          [&] { 
            bool canTransitTofloat = charger.isAbsorbingDisabled() && (charger.batteryAtFullCapacity || (sensor.getRawBatteryV() > floatV));
            return canTryToTransit && canTransitTofloat;
          },
          [&] { 
            //Serial.println("transit to float");
            return charger.goFloat(); }
        );

      return flow.get();
    }
