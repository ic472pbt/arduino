#ifndef SENSORSDATA_H
#include <Arduino.h>
#define SENSORSDATA_H
constexpr float FULL_BATT_VOLTS_RAW_PER_CELL = 141;   // raw value of fully charged battery voltage 12.6 
constexpr auto MAX_BATT_VOLTS_RAW_PER_CELL = 160; // 642       // raw value of battery voltage 14.35  ;
constexpr auto BATT_FLOAT_RAW_PER_CELL = 155; // 13.9 = 930  raw battery voltage we want to stop charging at;
constexpr auto LVR_PER_CELL = 2.1;
constexpr auto HVD_PER_CELL = 2.63;
constexpr auto LVD_PER_CELL = 1.8;
constexpr auto BAT_SENSOR_FACTOR = 0.01493012; // 0.02235088 19.23 = 1288;
constexpr auto CURRENT_OFFSET = 382 ;
constexpr auto CURRENT_IN_LOW_FACTOR = 0.009773528;
constexpr auto CURRENT_IN_FACTOR = 0.025429352; //<- GAIN2 GAIN1 -> 0.08180441 // 0.03828995 // 2A = 24;
constexpr auto BAT_24V_THRESHOLD_RAW = 1205;          // (18.0 / BAT_SENSOR_FACTOR)

class SensorsData {
public:
    // ==== PUBLIC READ-ONLY VALUES ====
    int   
        floatVoltageRaw,
        maxVoltageRaw,
        rawCurrentOut = 0;

    unsigned long rawPowerPrev = 0;
    unsigned long batteryUpdateCount = 0;

    float temperature    = 0.0;
    float currentLoad    = 0.0;

    float fullyChargedVoltage,
      batteryIsmooth = 0.0;

    float PVvoltage      = 0.0;
    float PVvoltageSmooth= 0.0;
    float PVvoltageFloat = 0.0;
    float LVR = 12.6;
    float HVD = 15.8;
    float LVD = 10.8;
    int floatVoltageLimitRaw;

    // ==== PUBLIC API ====
    void identifyCellCount(){
      cellCount = (rawBatteryV > BAT_24V_THRESHOLD_RAW) ? 12 : 6;
      floatVoltageRaw = BATT_FLOAT_RAW_PER_CELL * cellCount;
      maxVoltageRaw = MAX_BATT_VOLTS_RAW_PER_CELL * cellCount;
      floatVoltageLimitRaw = floatVoltageRaw;
      fullyChargedVoltage = FULL_BATT_VOLTS_RAW_PER_CELL * cellCount * BAT_SENSOR_FACTOR;
      LVR = LVR_PER_CELL * cellCount;
      HVD = HVD_PER_CELL * cellCount;
      LVD = LVD_PER_CELL * cellCount;
    }

    uint8_t getCellCount() const {
      return cellCount;
    }

    void setRawBatteryV(int raw) {
        // Sampling rate fixed at 25 Hz (40 ms). Use fixed IIR coefficients tuned for that rate.
        const float FAST_ALPHA = 0.15f; // stage1: remove PWM/ADC ripple (~3 Hz cutoff)
        const float SLOW_ALPHA = 0.08f; // stage2: smooth for display/control (~500 ms time constant)

        rawBatteryV = raw;
        batteryV = raw * BAT_SENSOR_FACTOR;
        batteryUpdateCount++;


        // Two-stage filtering:
        if(batteryVsmooth == 0.0f) {
            // Initialize both filters on first sample
            batteryVsmooth = batteryV;
            rawBatteryVfast = batteryV;
        } else {
            // Stage 1: Fast filter for local high-frequency noise
            rawBatteryVfast = IIRFast(rawBatteryVfast, batteryV, FAST_ALPHA);

            // Stage 2: Fixed slow filter for stable display/control
            batteryVsmooth = IIRFast(batteryVsmooth, rawBatteryVfast, SLOW_ALPHA);
        }
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

    // Get fast-filtered battery voltage for MPPT control
    // This value removes high-frequency noise while maintaining responsiveness
    float getBatteryVfast() const {
        return rawBatteryVfast;
    }

    // Get average sample interval in milliseconds
    // Useful for monitoring actual ADC update rate
    float getAvgSampleInterval() const {
        return avgSampleInterval;
    }

    // Get instantaneous sample rate in Hz
    float getSampleRate() const {
        return 25.0;
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
        rawPower = (unsigned long)rawBatteryVfast * max(0, rawCurrentIn);
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
    float rawBatteryVfast   = 0.0;  // Fast filtered value (Stage 1)
    float avgSampleInterval = 40.0f; // Running average of sample interval (ms)
    int   
      inCurrentOffset = CURRENT_OFFSET,
      rawCurrentIn  = 0;
    float currentInput     = 0.0;

    unsigned long rawPower = 0;
    
    // Stage 1: Fast IIR filter for high-frequency noise (ADC, PWM ripple)
    // Called at local sampling rate (25Hz). Coefficients are fixed for that rate.
    float IIRFast(float oldValue, float newValue, float alpha){
      return oldValue * (1.0f - alpha) + newValue * alpha;
    }
    
    // High-frequency IIR filter for current measurements (sampled at ~25Hz)

    float IIR2(float oldValue, float newValue){
      return oldValue * 0.95 + newValue * 0.05;
    }
};
#endif
