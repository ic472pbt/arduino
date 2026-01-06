#ifndef SENSORSDATA_H
#include <Arduino.h>
#define SENSORSDATA_H
constexpr auto MAX_BATT_VOLTS_RAW_PER_CELL = 160; // 642       // raw value of battery voltage 14.35  ;
constexpr auto BATT_FLOAT_RAW_PER_CELL = 151; // 13.8 = 924 // 617  raw battery voltage we want to stop charging at;
#define BAT_SENSOR_FACTOR 0.01493012 // 0.02235088 19.23 = 1288
#define CURRENT_OFFSET 382 
#define CURRENT_IN_LOW_FACTOR 0.009773528
#define CURRENT_IN_FACTOR 0.025429352 //<- GAIN2 GAIN1 -> 0.08180441 // 0.03828995 // 2A = 24
constexpr auto BAT_24V_THRESHOLD_RAW = 1205;          // (18.0 / BAT_SENSOR_FACTOR)

class SensorsData {
public:
    // ==== PUBLIC READ-ONLY VALUES ====
    int   
        floatVoltageRaw,
        maxVoltageRaw,
        rawCurrentOut = 0;

    unsigned long rawPowerPrev = 0;

    float temperature    = 0.0;
    float currentLoad    = 0.0;

    float batteryIsmooth = 0.0;

    float PVvoltage      = 0.0;
    float PVvoltageSmooth= 0.0;
    float PVvoltageFloat = 0.0;

    int floatVoltageLimitRaw;

    // ==== PUBLIC API ====
    void identifyCellCount(){
      cellCount = (rawBatteryV > BAT_24V_THRESHOLD_RAW) ? 12 : 6;
      floatVoltageRaw = BATT_FLOAT_RAW_PER_CELL * cellCount;
      maxVoltageRaw = MAX_BATT_VOLTS_RAW_PER_CELL * cellCount;
      floatVoltageLimitRaw = floatVoltageRaw;
    }

    uint8_t getCellCount() const {
      return cellCount;
    }

    void setRawBatteryV(int raw) {
        rawBatteryV = raw;
        batteryV = raw * BAT_SENSOR_FACTOR;
        batteryVsmooth = batteryVsmooth == 0.0 ? batteryV : IIR2(batteryVsmooth, batteryV);
    }

    int getRawBatteryV() const {
        return rawBatteryV;
    }

    float getBatteryV() const {
        return batteryV;
    }

    float getBatteryVsmooth() const {
        return batteryVsmooth;
    }

    void setRawCurrentIn(int raw) {
        rawCurrentIn = raw  - inCurrentOffset;
        if(currentGain == 2){
          currentInput = rawCurrentIn * CURRENT_IN_LOW_FACTOR;
          if(rawCurrentIn > 200) {currentGain = 1; inCurrentOffset = CURRENT_OFFSET/2;}
        }
        else {
          currentInput = rawCurrentIn * CURRENT_IN_FACTOR;         
          if(rawCurrentIn < 78) {currentGain = 2; inCurrentOffset = CURRENT_OFFSET;}
        }
        batteryIsmooth = batteryIsmooth == 0.0 ? currentInput : IIR2(batteryIsmooth, currentInput);
        rawPower = (unsigned long)rawBatteryV * max(0, rawCurrentIn);
    }

    float getCurrentInput() const{
      return currentInput;
    }

    int getRawCurrentIn() const{
      return rawCurrentIn  + inCurrentOffset;
    }

    unsigned long getRawPower() const {
      return rawPower;
    }

    uint8_t getCurrentGain() const {
      return currentGain;
    }

private:
    uint8_t 
      currentGain = 2,
      cellCount;
    int rawBatteryV = 0;
    float batteryV       = 0.0;
    float batteryVsmooth = 0.0;
    int   
      inCurrentOffset = CURRENT_OFFSET,
      rawCurrentIn  = 0;
    float currentInput     = 0.0;

    unsigned long rawPower = 0;
    float IIR2(float oldValue, float newValue){
      return oldValue * 0.992 + newValue * 0.008;
    }
};
#endif
