#define LVD 10.8
#define MAX_BAT_VOLTS_RAW 961 // 642       // raw value of battery voltage 14.35  
#define BATT_FLOAT 13.80            // battery voltage we want to stop charging at
#define BATT_FLOAT_RAW 924 // 617          // raw battery voltage we want to stop charging at
#define ABSORPTION_START_V 12.6     // switch to absorption mode if bellow
#define ABSORPTION_START_V_RAW 844 // 564  // switch to absorption mode if bellow
#define CURRENT_ABSOLUTE_MAX 40.0    //  CALIB PARAMETER - Maximum Current The System Can Handle
#define CURRENT_OFFSET 382 
#define OFF_NUM 9                   // number of iterations of off charger state
#define SELF_CONSUMPTION 0.045      // static loss (A)
#define ABSORPTION_TIME_LIMIT 7200000L  // 2h
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
#define PWM 9
#define LOAD 13

#define LOAD_V_SENSOR A0
#define BT1 A1
#define RT1 A2
#define RT2 A3
#define CURRENT_IN_SENSOR 2     // ADS SDA green
#define CURRENT_OUT_SENSOR 3    // ADS SCL brown
#define BAT_V_SENSOR 0          // ADS yellow
#define SOL_V_SENSOR 1          // ADS green thiсk

#define BAT_SENSOR_FACTOR 0.01493012 // 0.02235088 19.23 = 1288
#define SOL_V_SENSOR_FACTOR 0.04226373 // 0.06352661 // 19.23 = 455
#define CURRENT_OUT_FACTOR 0.02044296 // 0.04114097 // 1A = 48
#define CURRENT_IN_FACTOR 0.025429352 //<- GAIN2 GAIN1 -> 0.08180441 // 0.03828995 // 2A = 24
#define CURRENT_IN_LOW_FACTOR 0.009773528


#include <EEPROM.h>
#include <ADS1X15.h>
#include <TimerOne.h>

ADS1015 ADS(0x48);

// global variables

int inCurrentOffset = CURRENT_OFFSET,
    outCurrentOffset = 164;

// timers
unsigned long lastInfoTime = 0;
unsigned long dischargeStartTime;
unsigned long
  timeOn,                              // SYSTEM running time counter
  lastLinkActiveTime    = 0, 
  rawPower              = 0,
  absorptionStartTime   = 0,           //SYSTEM PARAMETER -
  absorptionAccTime     = 0;           //SYSTEM PARAMETER - total time of absorption

// MPPT
byte 
  currentADCpin      = 0, 
  mpptReached        = 1,
  ERR         = 0;           // SYSTEM PARAMETER - 
           
unsigned int 
  LCDmap[6],                 // LCD memory
  duty                = 0;          // pwm duty    
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
  
float 
  sol_watts;                     // SYSTEM PARAMETER - Input power (solar power) in Watts
              
bool
  LCDcycling     = true,
  finishEqualize = false,
  controlFloat = false;

int 
  floatVoltageRaw = MAX_BAT_VOLTS_RAW,           // float or absorb
  tempCompensationRaw   = 0,           // SYSTEM PARAMETER - Voltage offset for ambient temperature
  powerCompensation     = 0,
  rawBatteryV           = 0,           // SYSTEM PARAMETER - 
  rawSolarV             = 0,           // SYSTEM PARAMETER - 
  rawCurrentIn          = 0,           // SYSTEM PARAMETER - 
  rawCurrentOut         = 0,           // SYSTEM PARAMETER - 
  avgStoreTS            = 0,           // SYSTEM PARAMETER - Temperature Sensor uses non invasive averaging, this is used an accumulator for mean averaging
  avgCountTS             = 50;        //  CALIB PARAMETER - Temperature Sensor Average Sampling Count

unsigned int
  BTS                   = 0u,        // SYSTEM PARAMETER - Raw board temperature sensor ADC value
  TS                    = 0u;        // SYSTEM PARAMETER - Raw temperature sensor ADC value


float
  batteryVsmooth        = 0.0,        // smoothed battery voltage
  solarVsmooth          = 0.0,        // smoothed PV voltage
  batteryIsmooth        = 0.0,        // smoothed battery current
temperatureMax        = 60.0,          // USER PARAMETER - Overtemperature, System Shudown When Exceeded (deg C)
temperature           = 0.0,           // SYSTEM PARAMETER -
batteryV              = 0.0000,        // SYSTEM PARAMETER - Battery voltage    
solarV                = 0.0000,        // SYSTEM PARAMETER - PV voltage    
currentInput          = 0.0000,        // SYSTEM PARAMETER - Current to the battery
currentLoad           = 0.0,
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
todayOutAh             = 0.0000,     // SYSTEM PARAMETER - Energy Output for today (Amper-Hours)
vInSystemMin           = 10.000;       //  CALIB PARAMETER - 


enum button_mode {none, plus, minus, both} button_state;       // enumerated variable that holds state for buttons
enum charger_mode {off, on, bulk, bat_float, scanning} charger_state;    // enumerated variable that holds state for charger state machine
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
  Timer1.initialize(25);  // 25 us = 40 kHz / 17us = 60kHz / 20us = 50kHz / 33us = 30kHz
  Timer1.pwm(PWM, 0);
  ADS.begin();
  ADS.setGain(1);  // 4.096V max
  BTS = analogRead(RT1);
  TS = analogRead(RT2);
  SetTempCompensation();
  
  cli(); // disable global interrupts
  // initialize Timer1
  // TCCR1B = TCCR1B & B11111000 | B00000001; // PWM 31372.55 Hz on 9 10

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
  
  delay(200);
  rawBatteryV = ADS.readADC(BAT_V_SENSOR);
  rawSolarV =   ADS.readADC(SOL_V_SENSOR);
  batteryV = rawBatteryV * BAT_SENSOR_FACTOR;
  batteryVsmooth = batteryV;  
  solarVsmooth = rawSolarV * SOL_V_SENSOR_FACTOR;
  // ADS.setGain(1); // 4.096V max
  ADS.requestADC(currentADCpin);
}

void loop() {  
  static unsigned long mainTimestamp = 0;           //SYSTEM PARAMETER -
  
  unsigned long currentTime=millis();  
  Read_Sensors(currentTime);
 /* Serial.print(duty); Serial.print(" "); // Serial.print(ADS.readADC(CURRENT_IN_SENSOR));
Serial.print("load: "); 
Serial.print( rawCurrentIn);  Serial.print(" ");Serial.println(currentInput);
delay(200);*/ 
 
  Device_Protection(currentTime, batteryV); 
  int wait = charger_state == bat_float ? 5000 : 200;
  if(controlFloat){ wait = 0;controlFloat = false;}
  if(currentTime - mainTimestamp > wait){ // inertia delay
    Charging_Algorithm(solarV, currentTime);
    mainTimestamp = currentTime;
  }
  print_data(solarV, currentTime);
  // float loadV = load_voltage();
  
  monitor_button();
  LCDinfo(currentTime);
}



float Voltage2Temp(unsigned int sensor){
  float Tlog = logf(THERM_PULLUP_R/(1023.0/(float)sensor - 1.0));
  return (1.0/((51.73763267e-7 * Tlog * Tlog - 9.654661062e-4) * Tlog + 8.203940872e-3))-273.15;
}

unsigned int IIR(unsigned int oldValue, unsigned int newValue, unsigned int alpha, unsigned int Q){
  return (unsigned int)((alpha * (unsigned long)(oldValue) + (Q - alpha) * (unsigned long)(newValue)) / Q); // 128 = 110 + 18 -> alpha = 0.86 / 0.14
}

float IIR2(float oldValue, float newValue){
  return oldValue * 0.992 + newValue * 0.008;
}

void SetTempCompensation(){
  temperature = Voltage2Temp(TS);
  tempCompensationRaw = (int)((25.0 - temperature) * TEMP_COMPENSATION_CF / BAT_SENSOR_FACTOR);
}

void Read_Sensors(unsigned long currentTime){
  static byte currentGain = 2;
  static bool catchAbsorbtion = false;
  static unsigned long 
    prevRoutineMillis     = 0ul,
    catchAbsTime          = 0ul,
    lastTempTime          = 0ul;

  /////////// TEMPERATURE SENSORS /////////////
  if(currentTime - lastTempTime > 10000ul){
    //TEMPERATURE SENSORS - Lite Averaging
    
    TS =  IIR(TS, analogRead(RT2), 18, 128);
    BTS = IIR(BTS, analogRead(RT1), 18, 128);
    SetTempCompensation();
    OTE = Voltage2Temp(BTS) > temperatureMax;  // overheating protection
    lastTempTime = currentTime;
  }
  
  /////////// BATTERY SENSORS /////////////
  if(currentADCpin == BAT_V_SENSOR && ADS.isReady()){
    rawBatteryV = ADS.getValue(); // ADS.readADC(BAT_V_SENSOR); 
    currentADCpin += 1;
    ADS.requestADC(currentADCpin); // 10ms until read is ready
    batteryV = rawBatteryV * BAT_SENSOR_FACTOR;
    batteryVsmooth = IIR2(batteryVsmooth, batteryV);
    BNC = batteryV < vInSystemMin;  //BNC - BATTERY NOT CONNECTED     
    if(rawBatteryV < ABSORPTION_START_V_RAW) {
      if(catchAbsorbtion){
        if(currentTime - catchAbsTime > 3600000ul){ // should spent at least 1h bellow ABSORPTION_START_V to rise float voltage
          absorptionAccTime = 0;       
          floatVoltageRaw = MAX_BAT_VOLTS_RAW; 
          catchAbsorbtion = false;
          finishEqualize = catchAbsorbtion;        
        }      
      }
      else{
        catchAbsTime = currentTime;
        catchAbsorbtion = true;
      }
    } else catchAbsorbtion = false;
    // If we've charged the battery above the MAX voltage 0.4V
    if (rawBatteryV > MAX_BAT_VOLTS_RAW + 27) {
      charger_state = bat_float;
      duty = 300;
      set_pwm_duty(true); 
    }
  }
  
  /////////// PV SENSORS /////////////
  if(currentADCpin == SOL_V_SENSOR && ADS.isReady()){
    rawSolarV =  ADS.getValue(); // ADS.readADC(SOL_V_SENSOR);
    currentADCpin += 1;
    ADS.setGain(currentGain); // read current IN
    ADS.requestADC(currentADCpin);
    solarV = rawSolarV * SOL_V_SENSOR_FACTOR;
    solarVsmooth = IIR2(solarVsmooth, solarV);
    if(solarV + 0.5 < batteryV) {IUV=1;REC=1;}else{IUV=0;}   //IUV - INPUT UNDERVOLTAGE: Input voltage is below max battery charging voltage (for charger mode only)     
  }
  
  if(currentADCpin == CURRENT_IN_SENSOR && ADS.isReady()){
    rawCurrentIn = ADS.getValue() - inCurrentOffset;
    currentADCpin += 1;
    ADS.setGain(1);
    ADS.requestADC(currentADCpin);
    if(currentGain == 2){
      currentInput = rawCurrentIn * CURRENT_IN_LOW_FACTOR;
      if(rawCurrentIn > 200) {currentGain = 1; inCurrentOffset = CURRENT_OFFSET/2;}
    }
    else {
      currentInput = rawCurrentIn * CURRENT_IN_FACTOR;
     
      if(rawCurrentIn < 78) {currentGain = 2; inCurrentOffset = CURRENT_OFFSET;}
    }
    batteryIsmooth = IIR2(batteryIsmooth, currentInput);
    rawPower = rawBatteryV * rawCurrentIn;
    IOC = currentInput  > CURRENT_ABSOLUTE_MAX;  //IOC - INPUT  OVERCURRENT: Input current has reached absolute limit
  }
  
  sol_watts = max(batteryV*currentInput, 0.0);  // ignore negative power supply current
  powerCompensation = finishEqualize ? 0 : min(39, max(0, (int)((sol_watts - 60.0) * 0.001764706 / BAT_SENSOR_FACTOR)));

  /////////// LOAD SENSORS /////////////
  if(currentADCpin == CURRENT_OUT_SENSOR && ADS.isReady()){
    rawCurrentOut = ADS.getValue() - outCurrentOffset;
    currentADCpin = BAT_V_SENSOR;
    ADS.requestADC(currentADCpin);
    currentLoad = rawCurrentOut * CURRENT_OUT_FACTOR;
    OOC = currentLoad > CURRENT_ABSOLUTE_MAX;  //OOC - OUTPUT OVERCURRENT: Output current has reached absolute limit 
  }
  
   //TIME DEPENDENT SENSOR DATA COMPUTATION
  unsigned int 
    ct = millis(),
    time_span = ct - prevRoutineMillis;  

  if(time_span >= ROUTINE_INTERVAL){   //Run routine every millisRoutineInterval (ms)
    prevRoutineMillis = ct;                     //Store previous time
    float totalLoadCurrent = currentInput < 0 ? currentLoad - currentInput : currentLoad; // add self consumption
    
    todayOutWh += (batteryV * totalLoadCurrent * time_span) / 3.6E+6;
    todayOutAh += totalLoadCurrent * time_span / 3.6E+6;

    if(charger_state != off){
      todayWh += (sol_watts * time_span) / 3.6E+6;  //Accumulate and compute energy harvested (3600s*(1000/interval))
      todayAh += max(currentInput, 0.0) * time_span / 3.6E+6;
    }
    timeOn += time_span;
    daysRunning = timeOn * 1.1574074e-8; //Compute for days running / (86400s * 1000)                                                        //Increment time counter
  } 
}  

int curValue = 0;
int prevValue = 0;

void monitor_button(){
  
    button_mode value = read_button();
    if(prevValue-curValue > 200 && value == plus){      
      duty += 2;
      set_pwm_duty(false);
    }
    if(prevValue-curValue > 200 && value == minus){
      duty -= 2;
      set_pwm_duty(false);
    }
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
    outhAh += todayOutAh * 1.0E-3; 
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
        Serial.print("IN offst:");   Serial.print(inCurrentOffset);    
        Serial.print(" IN curr:");    Serial.print(rawCurrentIn + inCurrentOffset);    
        Serial.print(" OUT offst:");  Serial.print(outCurrentOffset);    
        Serial.print(" OUT curr:");   Serial.print(rawCurrentOut + outCurrentOffset);    
        Serial.print(" PWM:");        Serial.print(duty);
        Serial.print(" RT1:");        Serial.print(analogRead(RT1));    
        Serial.print(" RT2:");        Serial.print(analogRead(RT2));    
        Serial.print(" ABS(h):");     Serial.print((absorptionAccTime + currentTime - absorptionStartTime)/3600000.0, DEC);
        Serial.print(" Float V:");    Serial.print(floatVoltageRaw * BAT_SENSOR_FACTOR);
        Serial.print(" TCor V:");    Serial.print(tempCompensationRaw * BAT_SENSOR_FACTOR); // temperature correction
        Serial.print(" PCor V:");    Serial.print(-powerCompensation * BAT_SENSOR_FACTOR); // power correction
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
      Serial.print(" kWh:");       Serial.print(value);       
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);
      Serial.print(" hAh:");      Serial.print(value);       
      eeAddress += sizeof(float); 
      EEPROM.get(eeAddress, value); Serial.print(" TWh:");      Serial.print(value);       
      eeAddress += sizeof(float); 
      EEPROM.get(eeAddress, value);  Serial.print(" TAh:");      Serial.print(value);       
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);
      Serial.print(" oWh:");      Serial.print(value); 
      eeAddress += sizeof(float); EEPROM.get(eeAddress, value);            
      Serial.print(" oAh:");      Serial.print(value);       
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
        float solarCurrent = sol_watts/solarVoltage;
        Serial.print("Current (panel) = ");
        Serial.println(solarCurrent);
      //  Serial.print("      ");
      
        Serial.print("Voltage (panel) = "); Serial.println(solarVsmooth);
        
        Serial.print("Power (panel) = ");
        Serial.println(sol_watts);
      
        Serial.print("Battery Voltage = "); Serial.println(batteryVsmooth);
      
        Serial.print("Battery Current = "); Serial.println(batteryIsmooth);      
        Serial.print("Load Current = ");    Serial.println(currentLoad);
    
        Serial.print("Charging = ");
        if (charger_state == off) Serial.println("off  ");
        else if (charger_state == on) Serial.println("on ");
        else if (charger_state == bulk) Serial.println("bulk ");
        else if (charger_state == bat_float) Serial.println("float");
        else if (charger_state == scanning) Serial.println("scan");
        Serial.print("pwm = ");
        if(charger_state == off)
          Serial.println(0.0);
        else
          Serial.println(duty/10.23);
        
        Serial.print("OUT Temp = ");    Serial.println(temperature);
        Serial.print("SYS Temp = ");    Serial.print(Voltage2Temp(BTS));
    }
    else if(L=='s'){
      StoreHarvestingData(currentTime);
      Serial.print("ok");
    }
    else if(L=='r'){ //reset harvest data
      ResetHarvestingData();
      Serial.print("ok");
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
