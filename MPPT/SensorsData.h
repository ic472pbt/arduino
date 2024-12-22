#ifndef SENSORSDATA_H
#define SENSORSDATA_H
#define MAX_BAT_VOLTS_RAW 961 // 642       // raw value of battery voltage 14.35  

struct SensorsData {
    int
      floatVoltageRaw = MAX_BAT_VOLTS_RAW,  // float or absorb    
      rawBatteryV           = 0,           // SYSTEM PARAMETER - 
      rawCurrentIn          = 0,           // current to the battery
      rawCurrentOut         = 0;           // SYSTEM PARAMETER -     
    unsigned long 
      rawPower              = 0;  
    float
      temperature           = 0.0,           // SYSTEM PARAMETER -
      currentInput          = 0.0000,        // SYSTEM PARAMETER - Current to the battery
      currentLoad           = 0.0,
      batteryV              = 0.0000,        // SYSTEM PARAMETER - Battery voltage 
      batteryIsmooth        = 0.0,           // smoothed battery current
      batteryVsmooth        = 0.0,           // smoothed battery voltage
      PVvoltageSmooth       = 0.0,           // smoothed PV voltage
      PVvoltageFloat        = 0.0,           // float PV voltage
      PVvoltage             = 0.0000;        // SYSTEM PARAMETER - PV voltage   
};
#endif
