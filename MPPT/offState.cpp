#include "SensorsData.h"
#include "offState.h"
#include "Charger.h"

IState* offState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
      IState* newState = this;
      int floatV = charger.floatVoltageTempCorrectedRaw(sensor);

      charger.mpptReached = 0;
      if (charger.off_count > 0) {                                  // this means that we run through the off state OFF_NUM of times with out doing
          charger.off_count--;                                        // anything, this is to allow the battery voltage to settle down to see if the  
      }                                                     // battery has been disconnected
      else if ((sensor.rawBatteryV > floatV) && (sensor.PVvoltage > sensor.batteryV)) {
          newState = &charger.goFloat();                         // if battery voltage is still high and solar volts are high
      }    
      else if ((sensor.batteryV > LVD) && (sensor.rawBatteryV < floatV) && (sensor.PVvoltage > sensor.batteryV)) {
          newState = &charger.scanInstance;
          charger.pwmController.setMinDuty();
      }
      return newState;
    }
