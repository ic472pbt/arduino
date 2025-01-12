#define BATT_FLOAT 13.80            // battery voltage we want to stop charging at
#define ABSORPTION_START_V 12.6     // switch to absorption mode if bellow
#define ABSORPTION_START_V_RAW 844 // 564  // switch to absorption mode if bellow
#define CURRENT_ABSOLUTE_MAX 40.0    //  CALIB PARAMETER - Maximum Current The System Can Handle

#define SELF_CONSUMPTION 0.045      // static loss (A)
#define TEMP_COMPENSATION_CF 0.024     // V / 12V / degC / cp = 25deg
#define THERM_PULLUP_R 12000.0         // CALIB PARAMETER - pullup temp sensor's resistance. PullUp - NTC voltage divider
#define ROUTINE_INTERVAL 250           //  USER PARAMETER - Time Interval Refresh Rate For Routine Functions (ms)

#define IN_CURRENT_RECALIBRATE_INTERVAL 60000  //1 minute to recalibrate the shunt resistor
#define INFO_REPORT_INTERVAL 3000             //20 seconds between reports
#define COM1 2  // violet pin 3
#define COM2 3  // violet pin 2
#define COM3 4  // black pin 32
#define COM4 5  // brown pin 28
#define COM5 6  // red pin 27
#define COM6 7  // orange pin 26
#define LPl 8       // latch pin 595 left register
#define LPr 11      // latch pin 595 right register
#define CP 10       // clock pin 595 
#define DP 12       // data pin 595 
#define LOAD 13

#define LOAD_V_SENSOR A0
#define BT1 A1
#define CURRENT_IN_SENSOR 2     // ADS SDA green
#define CURRENT_OUT_SENSOR 3    // ADS SCL brown
#define BAT_V_SENSOR 0          // ADS yellow
#define SOL_V_SENSOR 1          // ADS green thiсk

#define BAT_SENSOR_FACTOR 0.01493012 // 0.02235088 19.23 = 1288
#define CURRENT_OUT_FACTOR 0.02044296 // 0.04114097 // 1A = 48
#define CURRENT_IN_FACTOR 0.025429352 //<- GAIN2 GAIN1 -> 0.08180441 // 0.03828995 // 2A = 24
#define CURRENT_IN_LOW_FACTOR 0.009773528

#define SS_RAMP_DELAY 20

#include "IIRFilter.h"
#include "Charger.h"
#include "Sensors.h"
#include <EEPROM.h>

IIRFilter dutyIIR(80, 120); 
IIRFilter TSIIR(110, 128); 
IIRFilter BTSIIR(18, 128); 
Charger charger(dutyIIR);
Sensors sensors(TSIIR, BTSIIR, charger);

// global variables



// timers
unsigned long lastInfoTime = 0;
unsigned long dischargeStartTime;
unsigned long
  timeOn,                              // SYSTEM running time counter
  lastLinkActiveTime    = 0; 
  
  
// MPPT
byte 
  versionNum         = 7,    // Firmware version.


  ERR         = 0;           // SYSTEM PARAMETER - 
           
unsigned int 
  pwmPeriod           = 17,          // us
  LCDmap[6];                         // LCD memory

bool
BNC                   = 0,           // SYSTEM PARAMETER -  
REC                   = 0,           // SYSTEM PARAMETER - 
FLV                   = 0,           // SYSTEM PARAMETER - 
IUV                   = 0,           // SYSTEM PARAMETER - 
IOV                   = 0,           // SYSTEM PARAMETER - 
IOC                   = 0,           // SYSTEM PARAMETER - 
OUV                   = 0,           // SYSTEM PARAMETER - 
OOV                   = 0,           // SYSTEM PARAMETER - 
OOC                   = 0,           // SYSTEM PARAMETER - 
OTE                   = 0;           // SYSTEM PARAMETER -
  
unsigned long 
  prevRoutineMillis     = 0ul;

              
bool
  LCDcycling     = true;

int   
  avgStoreTS            = 0,         // SYSTEM PARAMETER - Temperature Sensor uses non invasive averaging, this is used an accumulator for mean averaging
  avgCountTS            = 50;        //  CALIB PARAMETER - Temperature Sensor Average Sampling Count


float   
currentOutAbsolute     = 35.0000,      //  CALIB PARAMETER - Maximum Output Current The System Can Handle (A - Input)
totalDaysRunning      = 0.0,         // total days running
daysRunning           = 0.0000,      // SYSTEM PARAMETER - Stores the total number of days the MPPT device has been running since last powered
todayWh               = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy today
kWh                   = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy harvested (Kiliowatt-Hours)
hAh                   = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy harvested (hecto Amper-Hours)
outkWh                = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy supply (kWatt-Hours)
todayOutWh            = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy supply (Watt-Hours)
todayAh               = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy today
outhAh                 = 0.0000,     // SYSTEM PARAMETER - Energy Output total (hecto Amper-Hours)
todayOutAh             = 0.0000;     // SYSTEM PARAMETER - Energy Output for today (Amper-Hours)

enum button_mode {none, plus, minus, both} button_state;       // enumerated variable that holds state for buttons
enum valueType {voltage, degree, amper, percent, power, amperHour};      // value type LCD label right indicator

void setup() {
  for(int i = 2; i < 14; i++){pinMode(i, OUTPUT);} 
  digitalWrite(LOAD, LOW); // LOW - toggle load ON, HIGH - toggle load OFF, 21pin magenta  
  digitalWrite(LPl, HIGH); digitalWrite(LPr, HIGH);
  // managed by the Timer1 library analogWrite(PWM, 0);     // HIGH - toggle MOSFET ON, LOW - toggle MOSFET OFF
  pinMode(LOAD_V_SENSOR, INPUT); // high/low only 22pin red
  // pinMode(BAT_V_SENSOR, INPUT); // continuous 11pin yellow moved to ADC
  pinMode(RT1, INPUT); // temperature, 13pin blue + 595
  pinMode(RT2, INPUT); // temperature, 16pin brown + 595
  pinMode(BT1, INPUT); // buttons
  // pinMode(CURRENT_IN_SENSOR, INPUT); // inbound current moved to ADC
  // pinMode(CURRENT_OUT_SENSOR, INPUT); // outbound current moved to ADC
  // outCurrentOffset = 109; // zero current offset
  // pinMode(SOL_V_SENSOR, INPUT); // solar 8pin green moved to ADC
  Serial.begin(9600);
  Wire.begin();
  ReadHarvestingData();
  charger.initializePWM(pwmPeriod);  // 25 us = 40 kHz / 17us = 60kHz / 20us = 50kHz / 33us = 30kHz
  sensors.initialize();
  
  cli(); // disable global interrupts

  TCCR2A = 0; // Set entire TCCR2A register to 0
  TCCR2B = 0; // Same for TCCR2B
  
  // Set compare match register to desired timer count for 2ms
  OCR2A = 255; // (16MHz / (1000Hz * 64 prescaler)) - 1 (must be <256)
  
  // Turn on CTC mode
  TCCR2A |= (1 << WGM21);
  
  // Set CS22 bit for 64 prescaler
  TCCR2B |= (1 << CS22);
  
  // Enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);

  sei(); // enable global interrupts
  
  // delay(100);
  sensors.initialRead();
}

void loop() {  
  static unsigned long 
    lastRampUpTime = 0,
    lastSensorsUpdateTime = 0;
  static char ssDelta = 1;
  
  unsigned long currentTime=millis();  
  sensors.Read(currentTime);
  Read_Sensors(currentTime);
  /*Serial.print(duty); Serial.print(" "); // Serial.print(ADS.readADC(CURRENT_IN_SENSOR));
Serial.print("load: "); 
Serial.print( rawCurrentIn);  Serial.print(" ");Serial.println(currentInput);
delay(200);*/ 
 
  Device_Protection(currentTime, sensors.values.batteryV); 
  int sensorDelay = charger.currentState->isFloat() ? 5000 : 200;
  if(charger.controlFloat){ sensorDelay = 0;charger.controlFloat = false;}
  if(currentTime - lastSensorsUpdateTime > sensorDelay){ // sensors inertia delay
    charger.Charge(sensors.values, currentTime);
    lastSensorsUpdateTime = currentTime;
  }
  print_data(sensors.values.PVvoltage, currentTime);
  // float loadV = load_voltage();
  
  monitor_button();
  LCDinfo(currentTime);
}

void Read_Sensors(unsigned long currentTime){  
  static bool catchAbsorbtion = false;
  
  // disable for high accum capacity
  // powerCompensation = 0; 
  
   //TIME DEPENDENT SENSOR DATA COMPUTATION
  unsigned int 
    ct = millis(),
    time_span = ct - prevRoutineMillis;  

  if(time_span >= ROUTINE_INTERVAL){   //Run routine every millisRoutineInterval (ms)
    prevRoutineMillis = ct;                     //Store previous time
    float totalLoadCurrent = sensors.values.currentInput < 0 ? sensors.values.currentLoad - sensors.values.currentInput : sensors.values.currentLoad; // add self consumption
    
    todayOutWh += (sensors.values.batteryV * totalLoadCurrent * time_span) / 3.6E+6;
    todayOutAh += totalLoadCurrent * time_span / 3.6E+6;

    if(!charger.currentState->isOff()){
      todayWh += (charger.sol_watts * time_span) / 3.6E+6;  //Accumulate and compute energy harvested (3600s*(1000/interval))
      todayAh += max(sensors.values.currentInput, 0.0) * time_span / 3.6E+6;
    }
    timeOn += time_span;
    daysRunning = timeOn * 1.1574074e-8; //Compute for days running / (86400s * 1000)                                                        //Increment time counter
  } 
}  

int curValue = 0;
int prevValue = 0;

void monitor_button(){
  
    button_mode value = read_button();
    if(prevValue-curValue > 200 && value == plus) charger.pwmController.setPeriod(++pwmPeriod); //charger.pwmController.incrementDuty(2);
    if(prevValue-curValue > 200 && value == minus) charger.pwmController.setPeriod(--pwmPeriod); // charger.pwmController.incrementDuty(-2);
    if(prevValue-curValue > 200 && value == both) LCDcycling = !LCDcycling;
    prevValue = curValue;
}

button_mode read_button(){
  // 0 1016 / + 707 / - 565 / +- 454
  int value = analogRead(BT1);
  curValue = value;
  if(value > 862) return none;
  else if(value > 636) return plus;
  else if(value > 509) return minus;
       else return both;
}

float load_voltage(){
  int value = analogRead(LOAD_V_SENSOR);
  float voltage = (8.483633388E-06 * value + 2.238116288E-03) * value + 5.461806416E+00; 
  //Serial.print("LOADV ");Serial.print(value);Serial.print(" ");Serial.println(voltage); 
  return voltage;
}

void ResetHarvestingData(){
    kWh = 0.0;         hAh = 0.0;
    outkWh = 0.0;
    todayWh = 0.0;     todayAh = 0.0;
    todayOutWh = 0.0;  todayOutAh = 0.0;
    timeOn = 0; totalDaysRunning = 0.0;
    
    int eeAddress = 0;
    EEPROM.put(eeAddress, 0.0); // clear kWh
    
    eeAddress += sizeof(float); // clear hAh
    EEPROM.put(eeAddress, 0.0);  
    
    eeAddress += sizeof(float) * 3; // clear outkWh
    EEPROM.put(eeAddress, 0.0);  
    eeAddress += sizeof(float); // clear outhAh
    EEPROM.put(eeAddress, 0.0);  

    eeAddress += sizeof(float) * 3;
    EEPROM.put(eeAddress, 0.0);  // clear total days
}

void StoreHarvestingData(unsigned long currentTime){
    static unsigned long lastSaveTime = 0;

    if(currentTime - lastSaveTime < 3600000L) return;
    lastSaveTime = currentTime;
    // Store harvest data
    kWh += todayWh * 1.0E-3; 
    outkWh += todayOutWh * 1.0E-3; 
    hAh += todayAh * 1.0E-2; 
    outhAh += todayOutAh * 1.0E-2; 
    totalDaysRunning += daysRunning;
    int eeAddress = 0;
    EEPROM.put(eeAddress, kWh);
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, hAh);  
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, todayWh);  
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, todayAh);      
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, outkWh);
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, outhAh);  
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, todayOutWh);  
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, todayOutAh);  
    eeAddress += sizeof(float);
    EEPROM.put(eeAddress, totalDaysRunning);  
    todayWh = 0.0;
    todayAh = 0.0;   
    todayOutWh = 0.0;
    todayOutAh = 0.0;
    timeOn = 0;
}

void ReadHarvestingData(){
// Read harvest data
  int eeAddress = 0;
  EEPROM.get(eeAddress, kWh);       if(isnan(kWh)) kWh = 0.0;
  eeAddress += sizeof(float);
  EEPROM.get(eeAddress, hAh);       if(isnan(hAh)) hAh = 0.0;  
  eeAddress += sizeof(float);
  // EEPROM.get(eeAddress, todayWh);  if(isnan(todayWh)) todayWh = 0.0;    
  eeAddress += sizeof(float);
  // EEPROM.get(eeAddress, todayAh);  if(isnan(todayAh)) todayAh = 0.0;    
  eeAddress += sizeof(float);
  EEPROM.get(eeAddress, outkWh);    if(isnan(outkWh)) outkWh = 0.0;
  eeAddress += sizeof(float);
  EEPROM.get(eeAddress, outhAh);    if(isnan(outhAh)) outhAh = 0.0;  
  eeAddress += sizeof(float);
  // EEPROM.get(eeAddress, todayOutWh);if(isnan(todayOutWh)) todayOutWh = 0.0;    
  eeAddress += sizeof(float);
  // EEPROM.get(eeAddress, todayOutAh);  if(isnan(todayOutAh)) todayOutAh = 0.0;    
  eeAddress += sizeof(float);
  EEPROM.get(eeAddress, totalDaysRunning);  if(isnan(totalDaysRunning)) totalDaysRunning = 0.0;    
}


//------------------------------------------------------------------------------------------------------
// This routine prints all the data out to the serial port.
//------------------------------------------------------------------------------------------------------
void print_data(float solarVoltage, unsigned long currentTime){  
  if(currentTime-lastInfoTime < INFO_REPORT_INTERVAL) return; // skip the cycle
  bool finalize = false;
  bool 
    harvest     = false,
    info        = false,
    erors       = false,
    save        = false;
  while(Serial.available()>0)
  {
    finalize = true;
    lastLinkActiveTime = currentTime;
    char L = Serial.read();        
    if(L=='c'){ // calibration data request      
        Serial.print(" mDuty:");      Serial.print(charger.pwmController.mpptDuty);    
        Serial.print(" IN curr:");    Serial.print(sensors.values.rawCurrentIn + sensors.inCurrentOffset);    
        Serial.print(" OUT curr:");   Serial.print(sensors.values.rawCurrentOut + sensors.outCurrentOffset);    
        Serial.print(" PWM:");        Serial.print(charger.pwmController.duty);
        Serial.print(" RT1:");        Serial.print(analogRead(RT1));    
        Serial.print(" f:");          Serial.print(charger.frequency());    
        Serial.print(" ABS(h):");     Serial.print(charger.absorptionAccTime/3600000.0);
        Serial.print(" Float V:");   Serial.print(sensors.values.floatVoltageRaw * BAT_SENSOR_FACTOR);
        Serial.print(" TCor V:");    Serial.print(charger.tempCompensationRaw * BAT_SENSOR_FACTOR); // temperature correction
        Serial.print(" PCor V:");    Serial.print(-charger.powerCompensation * BAT_SENSOR_FACTOR); // power correction
        Serial.print(" sDown:");     Serial.print(charger.stepsDown);    
    }
    else if(L=='e'){ // errors request
        Serial.print(" ERR:");   Serial.print(ERR);
        Serial.print(" FLV:");   Serial.print(FLV);  
        Serial.print(" BNC:");   Serial.print(BNC);  
        Serial.print(" IUV:");   Serial.print(IUV); 
        Serial.print(" IOC:");   Serial.print(IOC); 
        Serial.print(" OOV:");   Serial.print(OOV); 
        Serial.print(" OOC:");   Serial.print(OOC);
        Serial.print(" OTE:");   Serial.print(OTE); 
        Serial.print(" REC:");   Serial.print(REC);       
    }
    else if(L == 'y'){ // get yesterday's data
      int eeAddress = 0;
      float value;
      EEPROM.get(eeAddress, value);
      Serial.print(" >kWh:");       Serial.print(value);       
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);
      Serial.print(" >hAh:");      Serial.print(value);       
      eeAddress += sizeof(float); 
      EEPROM.get(eeAddress, value); Serial.print(" TWh:");      Serial.print(value);       
      eeAddress += sizeof(float); 
      EEPROM.get(eeAddress, value);  Serial.print(" TAh:");      Serial.print(value);       
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);
      Serial.print(" kWh>:");      Serial.print(value); 
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);            
      Serial.print(" hAh>:");      Serial.print(value);       
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);   
      Serial.print(" ToWh:");     Serial.print(value);   
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);    
      Serial.print(" ToAh:");     Serial.print(value);          
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);    
      Serial.print(" days:");     Serial.print(value);          
    }
    else if(L == 'h'){ // harvest request
        Serial.print(" >kWh:");   Serial.print(kWh);       
        Serial.print(" >hAh:");   Serial.print(hAh);       
        Serial.print(" TWh:");   Serial.print(todayWh);       
        Serial.print(" TAh:");    Serial.print(todayAh);       
        Serial.print(" kWh>:");    Serial.print(outkWh);       
        Serial.print(" hAh>:");    Serial.print(outhAh);       
        Serial.print(" ToWh:");    Serial.print(todayOutWh);       
        Serial.print(" TohAh:");    Serial.print(todayOutAh);       
        Serial.print(" Days:");  Serial.print(daysRunning);       
    }
    else if(L=='i'){ // information request        
        float solarCurrent = charger.sol_watts/solarVoltage;
        Serial.print("Current (panel) = ");
        Serial.println(solarCurrent);
      //  Serial.print("      ");
      
        Serial.print("Voltage (panel) = "); Serial.println(sensors.values.PVvoltageSmooth);        
        Serial.print("Power (panel) = ");   Serial.println(charger.sol_watts);      
        Serial.print("Battery Voltage = "); Serial.println(sensors.values.batteryVsmooth);      
        Serial.print("Battery Current = "); Serial.println(sensors.values.batteryIsmooth);      
        Serial.print("Load Current = ");    Serial.println(sensors.values.currentLoad);    
        Serial.print("Charging = ");        Serial.println(charger.currentState->GetName());
        Serial.print("pwm = ");
        if(charger.currentState->isOff())
          Serial.println(0.0);
        else
          Serial.println(charger.pwmController.duty/10.23);
        
        Serial.print("OUT Temp = ");    Serial.println(sensors.values.temperature);
        Serial.print("SYS Temp = ");    Serial.println(sensors.boardTemperature());
        Serial.print("PVF = ");    Serial.print(sensors.values.PVvoltageFloat);
    }
    else if(L=='s'){
      StoreHarvestingData(currentTime);
      Serial.print("ok");
    }
    else if(L=='r'){ //reset harvest data
      ResetHarvestingData();
      Serial.print("ok");
    }
    else if(L=='v'){ //firmware version number
      Serial.print(versionNum);
    }
    else{
      Serial.print("UC"); //unknown command 
    }
    Serial.println();
  }
  if(finalize){
      Serial.println("#");
      lastInfoTime = currentTime;
  }
}
