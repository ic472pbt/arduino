#include "SensorsData.h"
#include "offState.h"
#include "Charger.h"

IState* offState::Handle(Charger& charger, SensorsData& sensor, unsigned long currentTime) 
{
      IState* newState = this;
      bool solarOff = true;
      int floatV = sensor.floatVoltageRaw + charger.tempCompensationRaw;

      charger.mpptReached = 0;
      if (charger.off_count > 0) {                                  // this means that we run through the off state OFF_NUM of times with out doing
          charger.off_count--;                                        // anything, this is to allow the battery voltage to settle down to see if the  
      }                                                     // battery has been disconnected
      else if ((sensor.rawBatteryV > floatV) && (sensor.PVvoltage > sensor.batteryV)) {
          newState = &charger.floatInstance;                         // if battery voltage is still high and solar volts are high
          solarOff = false;
      }    
      else if ((sensor.batteryV > LVD) && (sensor.rawBatteryV < floatV) && (sensor.PVvoltage > sensor.batteryV)) {
          newState = &charger.scanInstance;
          solarOff = false;
          charger.pwmController.setMinDuty();
      }
      if(solarOff) charger.pwmController.shutdown();  
      return newState;
    }
