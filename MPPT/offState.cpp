#include "SensorsData.h"
#include "offState.h"
#include "Charger.h"

IState* offState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
      IState* newState = this;

      if (currentTime - offTime > OFF_MIN_INTERVAL) { 
        int floatV = charger.floatVoltageTempCorrectedRaw(sensor);
        if ((sensor.batteryV > LVD) && (sensor.rawBatteryV < floatV) && (sensor.PVvoltage > charger.minPVVoltage)) {
            newState = charger.goScan(false);
        } else if (charger.batteryAtFullCapacity || ((sensor.rawBatteryV > floatV) && (sensor.PVvoltage > sensor.batteryV))) {
            newState = charger.goFloat();                         
        }
      }                                                   

      return newState;
    }
