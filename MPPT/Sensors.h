#include "Arduino.h"
#define RT1 A2
#define RT2 A3
#define BOARD_TEMP_OFFSET -1.72              // CALIB PARAMETER offset board temperature relative to ambient
#define MAX_BOARD_TEMPERATURE 60.0          // USER PARAMETER - Overtemperature, System Shudown When Exceeded (deg C)
#define MIN_SYSTEM_VOLTAGE    10.0          //  CALIB PARAMETER - 
#define SOL_V_SENSOR_FACTOR 0.04226373 // 0.06352661 // 19.23 = 455
#define CURRENT_OFFSET 382 
#define ONE_SECOND 1000ul                   // 1000 ms 
#define TEN_SECONDS 10000ul                 // 10000 ms 

#include "SensorsData.h"
#include "Charger.h"
#include <ADS1X15.h>

extern bool OTE, BNC, IUV, IOC, OOC;

class Sensors {
  private:
    ADS1015 
      ADS;
    IIRFilter& 
      TSFilter, BTSFilter;  
    Charger& 
      charger;
    bool
      sensorsUpdated = false;  // full cycle reading completed
    byte
      currentADCpin  = 0,
      currentGain    = 2;
    unsigned int
      BTS                   = 0u,        // SYSTEM PARAMETER - Raw board temperature sensor ADC value
      TS                    = 0u;        // SYSTEM PARAMETER - Raw temperature sensor ADC value
    unsigned long 
      powerProbeTime        = 0ul,
      catchAbsTime          = 0ul,
      lastTempTime          = 0ul;

    float IIR2(float oldValue, float newValue){
      return oldValue * 0.992 + newValue * 0.008;
    }
    float Voltage2Temp(unsigned int sensor){
      float Tlog = logf(THERM_PULLUP_R/(1023.0/(float)sensor - 1.0));
      return (1.0/((51.73763267e-7 * Tlog * Tlog - 9.654661062e-4) * Tlog + 8.203940872e-3))-273.15;
    }

  public:
    int 
      inCurrentOffset = CURRENT_OFFSET,
      outCurrentOffset = 164;  

    SensorsData values;
    
    Sensors(IIRFilter& TSFilterInstance, IIRFilter& BTSFilterInstance, Charger& chargerInstance) : 
      TSFilter(TSFilterInstance), BTSFilter(BTSFilterInstance), charger(chargerInstance), ADS(0x48) {}   

    void Read(unsigned long currentTime){
      /////////// TEMPERATURE SENSORS /////////////
      if(currentTime - lastTempTime > TEN_SECONDS){
        //TEMPERATURE SENSORS - Lite Averaging
        
        TS =  TSFilter.smooth(analogRead(RT2));
        BTS = BTSFilter.smooth(analogRead(RT1));
        SetTempCompensation();
        OTE = boardTemperature() > MAX_BOARD_TEMPERATURE;  // overheating protection
        lastTempTime = currentTime;
      }

      /////////// BATTERY SENSORS /////////////
      if(currentADCpin == BAT_V_SENSOR && ADS.isReady()){
        values.rawBatteryV = ADS.getValue(); // ADS.readADC(BAT_V_SENSOR); 
        currentADCpin += 1;
        ADS.requestADC(currentADCpin); // 10ms until read is ready
        values.batteryV = values.rawBatteryV * BAT_SENSOR_FACTOR;
        values.batteryVsmooth = IIR2(values.batteryVsmooth, values.batteryV);
        BNC = values.batteryV < MIN_SYSTEM_VOLTAGE;  //BNC - BATTERY NOT CONNECTED     
    
        // If we've charged the battery above the MAX voltage 0.4V rising overpower event
        if (values.rawBatteryV > MAX_BAT_VOLTS_RAW + 27) {
          if(charger.stepsDown <= 40) charger.stepsDown += 4;
          charger.pwmController.incrementDuty(-charger.stepsDown * 4);
          charger.setMaxFloatCurrent(values.currentInput * 0.66);
          charger.powerCapMode = true;      
          charger.currentState = charger.goOff(currentTime);
        }
      }

      /////////// PV SENSORS /////////////
      if(currentADCpin == SOL_V_SENSOR && ADS.isReady()){
        charger.rawSolarV =  ADS.getValue(); // ADS.readADC(SOL_V_SENSOR);
        currentADCpin += 1;
        ADS.setGain(currentGain); // read current IN
        ADS.requestADC(currentADCpin);
        values.PVvoltage = charger.rawSolarV * SOL_V_SENSOR_FACTOR;
        if(charger.pwmController.isShuteddown()) values.PVvoltageFloat = values.PVvoltage; // update float PV value
        values.PVvoltageSmooth = IIR2(values.PVvoltageSmooth, values.PVvoltage);
        if(values.PVvoltage + 0.5 < values.batteryV) {IUV=1;REC=1;}else{IUV=0;}   //IUV - INPUT UNDERVOLTAGE: Input voltage is below max battery charging voltage (for charger mode only)     
      }
      
      if(currentADCpin == CURRENT_IN_SENSOR && ADS.isReady()){
        values.rawCurrentIn = ADS.getValue() - inCurrentOffset;
        currentADCpin += 1;
        ADS.setGain(1);
        ADS.requestADC(currentADCpin);
        if(currentGain == 2){
          values.currentInput = values.rawCurrentIn * CURRENT_IN_LOW_FACTOR;
          if(values.rawCurrentIn > 200) {currentGain = 1; inCurrentOffset = CURRENT_OFFSET/2;}
        }
        else {
          values.currentInput = values.rawCurrentIn * CURRENT_IN_FACTOR;         
          if(values.rawCurrentIn < 78) {currentGain = 2; inCurrentOffset = CURRENT_OFFSET;}
        }
        values.batteryIsmooth = IIR2(values.batteryIsmooth, values.currentInput);
        values.rawPower = (unsigned long)values.rawBatteryV * values.rawCurrentIn;
        sensorsUpdated = true;
        if(currentTime - powerProbeTime >= ONE_SECOND){
          values.rawPowerPrev = values.rawPower;
          powerProbeTime = currentTime;
        }
        
        IOC = values.currentInput  > CURRENT_ABSOLUTE_MAX;  //IOC - INPUT  OVERCURRENT: Input current has reached absolute limit

        // update power value
        charger.sol_watts = max(values.batteryV * values.currentInput, 0.0);  // ignore negative power supply current

        // enable power correction bias to mitigate overproduction issue 
        charger.powerCompensation = 
          (charger.finishEqualize || !charger.stepsDown > 0) ? 0 
          : min(39, max(0, (int)(0.5 * (charger.sol_watts - 90.0) * 0.001764706 / BAT_SENSOR_FACTOR)));
      }
      
      /////////// LOAD SENSORS /////////////
      if(currentADCpin == CURRENT_OUT_SENSOR && ADS.isReady()){
        values.rawCurrentOut = ADS.getValue() - outCurrentOffset;
        currentADCpin = BAT_V_SENSOR;
        ADS.requestADC(currentADCpin);
        values.currentLoad = values.rawCurrentOut * CURRENT_OUT_FACTOR;
        OOC = values.currentLoad > CURRENT_ABSOLUTE_MAX;  //OOC - OUTPUT OVERCURRENT: Output current has reached absolute limit 
      }

    }
    
    void SetTempCompensation(){
      values.temperature = Voltage2Temp(TS);
      charger.tempCompensationRaw = (int)((25.0 - values.temperature) * TEMP_COMPENSATION_CF / BAT_SENSOR_FACTOR);
    }

    // convert raw int system temperature to float
    float boardTemperature(){
      return Voltage2Temp(BTS) + BOARD_TEMP_OFFSET;
    }

    void initialize(){
      ADS.begin();
      ADS.setGain(1);  // 4.096V max
    }

    void initialRead(){
      TS = analogRead(RT2);
      BTS = analogRead(RT1);
      TSFilter.reset(TS);
      BTSFilter.reset(BTS);      
      SetTempCompensation();
      values.rawBatteryV = ADS.readADC(BAT_V_SENSOR);
      charger.rawSolarV =   ADS.readADC(SOL_V_SENSOR);
      values.batteryV = values.rawBatteryV * BAT_SENSOR_FACTOR;
      values.batteryVsmooth = values.batteryV;  
      values.PVvoltageSmooth = charger.rawSolarV * SOL_V_SENSOR_FACTOR;
      // ADS.setGain(1); // 4.096V max
      ADS.requestADC(currentADCpin);
    }

    // Detect full cycle of sensors readings
    bool SensorsRoundCompleted(){
      if(sensorsUpdated){
        sensorsUpdated = false;
        return true;
      } else return sensorsUpdated;
    }
};
