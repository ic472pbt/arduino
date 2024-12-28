#include "SensorsData.h"
#include "offState.h"
#include "Charger.h"

IState* offState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
      IState* newState = this;;
      int floatV = charger.floatVoltageTempCorrectedRaw(sensor);

      charger.mpptReached = 0;
      if (currentTime - offTime > OFF_MIN_INTERVAL) {                              
        if ((sensor.rawBatteryV > floatV) && (sensor.PVvoltage > sensor.batteryV)) {
            newState = charger.goFloat();                         
        } else if ((sensor.batteryV > LVD) && (sensor.rawBatteryV < floatV) && (sensor.PVvoltage > sensor.batteryV)) {
            newState = charger.goScan();
        }                                    
      }                                                   

      return newState;
    }
