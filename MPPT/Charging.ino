#define LOW_SOL_WATTS 5.00         // low bulk mode limit
#define MIN_SOL_WATTS 1.00          // value of solar watts // this is 0.00 watts
#define MAX_PWM_DELTA 4
#define MIN_ACTIVE_DUTY 300              // minimum duty sensitive to current change
#define SCAN_STEP 90                     // rough MPPT searching step
#define ABSORPTION_TIME_LIMIT 14400000L  // max 4h of topping up per day

bool startTracking = true;
byte off_count = OFF_NUM;

void set_pwm_duty(bool solarOff) {
  if(solarOff) Timer1.pwm(PWM, 0);
  else{
    duty = min(1023, max(5, duty));        // check limits of PWM duty cyle and set to PWM_MAX
                                          // if pwm is less than PWM_MIN then set it to PWM_MIN
    Timer1.pwm(PWM, duty);
  }
} 

void switchToFloat(){
  mpptDuty = duty;
  controlFloat = true;                                // else if the battery voltage has gotten above the float
  charger_state = bat_float;                          // battery float voltage go to the charger battery float state            
}

void scan(){
  static byte cycleNum = 0;
  static unsigned long bestPower = 0;
  static unsigned int bestDuty = 0;
   
  if(cycleNum == 10){
    cycleNum = 0;
    bestPower = 0;
    duty = bestDuty;
    charger_state = bulk;
    startTracking = true;
  }else{
    cycleNum++;
    unsigned long solarPower = (unsigned long)rawCurrentIn * rawBatteryV;
    if(solarPower > bestPower){
      bestPower = solarPower;
      bestDuty = duty;
    }
    duty += SCAN_STEP;    
  }
  set_pwm_duty(false);
}

//  Off State - This is state that the charger enters when solar watts < MIN_SOL_WATTS. The charger goes into this
//      off in this state so that power from the battery doesn't leak back into the solar panel. 
void offHandle(float sol_volts){
  bool solarOff = true;
  int floatV = floatVoltageRaw + tempCompensationRaw;

  mpptReached = 0;
        if (off_count > 0) {                                  // this means that we run through the off state OFF_NUM of times with out doing
          off_count--;                                        // anything, this is to allow the battery voltage to settle down to see if the  
        }                                                     // battery has been disconnected
        else if ((rawBatteryV > floatV) && (sol_volts > batteryV)) {
          charger_state = bat_float;                          // if battery voltage is still high and solar volts are high
          solarOff = false;
        }    
        else if ((batteryV > LVD) && (rawBatteryV < floatV) && (sol_volts > batteryV)) {
          charger_state = scanning;
          solarOff = false;
          duty = MIN_ACTIVE_DUTY;
        }
        set_pwm_duty(solarOff);  
}

//------------------------------------------------------------------------------------------------------
// This routine is the charger state machine. It has four states on, off, bulk and float.
// It's called once each time through the main loop to see what state the charger should be in.
// The battery charger can be in one of the following four states:
// 
//  On State - this is charger state for MIN_SOL_WATTS < solar watts < LOW_SOL_WATTS. In this state is the solar 
//      watts input is too low for the bulk charging state but not low enough to go into the off state. 
//      In this state we just set the pwm = 99.9% to get the most of low amount of power available.

//  Bulk State - this is charger state for solar watts > MIN_SOL_WATTS. This is where we do the bulk of the battery
//      charging and where we run the Peak Power Tracking alogorithm. In this state we try and run the maximum amount
//      of current that the solar panels are generating into the battery.

//  Float State - As the battery charges it's voltage rises. When it gets to the MAX_BAT_VOLTS we are done with the 
//      bulk battery charging and enter the battery float state. In this state we try and keep the battery voltage
//      at MAX_BAT_VOLTS by adjusting the pwm value. If we get to pwm = 100% it means we can't keep the battery 
//      voltage at MAX_BAT_VOLTS which probably means the battery is being drawn down by some load so we need to back
//      into the bulk charging mode.

//------------------------------------------------------------------------------------------------------
void Charging_Algorithm(float sol_volts, unsigned long currentTime) { 
  static int 
    voltageInputPrev = 0,
    delta = 10;
  static byte 
    flip = 1,
    stepSize = MAX_PWM_DELTA;  
  static unsigned long
    rawPowerPrev   = 0,
    lastTrackingTime   = 0;
  // rawPowerOld        = 0;    // solar watts from previous time through ppt routine 
  
  if(ERR>0){
    charger_state = off;
    duty-=100;
    set_pwm_duty(true);
    return;
  }else if (REC==1){ // wait for recovery from low solar voltage (starting a new day)
      REC=0;
//      while(ADS.isBusy()){}
//      inCurrentOffset = ADS.readADC(CURRENT_IN_SENSOR) + 2;   // zero current offset
//      ADS.requestADC(currentADCpin);    
      duty = MIN_ACTIVE_DUTY;  
      charger_state = bat_float;
      StoreHarvestingData(currentTime);
      // allow absorbtion
      absorptionAccTime = 0;       
      absorptionStartTime = 0;
      floatVoltageRaw = MAX_BAT_VOLTS_RAW; 
      finishEqualize = false; 
      powerCapMode = false;
    }
  
    if(absorptionAccTime >= ABSORPTION_TIME_LIMIT) {
      floatVoltageRaw = BATT_FLOAT_RAW;  
      finishEqualize = true;
    }

    if(charger_state==off){                               // when we jump into the charger off state, off_count is set with OFF_NUM
        offHandle(sol_volts);
    }
    else if(charger_state==scanning){
        scan();
    } 
    else if(charger_state==on){                              
        mpptReached = 0;
        if (solarV + 0.2 < batteryV) {                      // if watts input from the solar panel is less than
          charger_state = off;                                // the minimum solar watts then 
          off_count = OFF_NUM;                                // go to the charger off state
          set_pwm_duty(true); 
          /* Serial.print(solarV); Serial.print(' ');
          Serial.print(batteryV);
          Serial.println("go off");*/
        }
        else if (rawBatteryV > (BATT_FLOAT_RAW - 7)) {            // else if the battery voltage has gotten above the float
          charger_state = bat_float;                          // battery float voltage go to the charger battery float state
          /*Serial.print(batteryV); Serial.print(' ');
          Serial.print(BATT_FLOAT);
          Serial.println("go float");*/
        }
        else if (sol_watts < LOW_SOL_WATTS) {                 // else if the solar input watts is less than low solar watts
          duty = 1023;                                      // it means there is not much power being generated by the solar panel
          set_pwm_duty(false);                                 // so we just set the pwm = 100% so we can get as much of this power as possible
          /*Serial.print(batteryV); Serial.print(' ');
          Serial.println("go on");*/
        }                                                     // and stay in the charger on state
        else {              
          // else if we are making more power than low solar watts figure out what the pwm
          charger_state = scanning;
          startTracking = true;
          duty = MIN_ACTIVE_DUTY;
          storeDuty = duty;
          set_pwm_duty(false);
          // stepSize = 16; flip = 1;
          /* transitionFromOnToBulk = true;
          // value should be and change the charger to bulk state 
          Serial.print(batteryV); Serial.print(' ');
          Serial.println("go bulk");*/
        }
    }      
    else if (charger_state==bulk){       
        if (solarV + 0.2 < batteryV) {                        // if watts input from the solar panel is less than
          charger_state = off;                                // the minimum solar watts then it is getting dark so
          off_count = OFF_NUM;                                // go to the charger off state
          set_pwm_duty(true); 
        }
        else if (rawBatteryV > floatVoltageRaw + tempCompensationRaw - powerCompensation){
          switchToFloat();
        }
                          
        else if (sol_watts < LOW_SOL_WATTS) {                 // else if the solar input watts is less than low solar watts
          charger_state = on;                                 // it means there is not much power being generated by the solar panel
          duty = 1023;
          // so go to charger on state
          set_pwm_duty(false);
        }
        else {                                    // this is where we do the Peak Power Tracking ro Maximum Power Point algorithm
          unsigned long solarPower = (unsigned long)rawCurrentIn * rawBatteryV;
          if(mpptReached == 1 || startTracking){
            if(currentTime - lastTrackingTime > 29900 || startTracking){
               // do perturbation
              stepSize = MAX_PWM_DELTA;
              delta = (2 * flip - 1) * stepSize;
              storeDuty = duty;
              duty += delta;
              lastTrackingTime = currentTime;
              mpptReached = 0; // ! reset MPPT
              rawPowerPrev = solarPower;   
              voltageInputPrev = rawSolarV;           
              startTracking = false;
            }
          }else{
        /*   Serial.print("power before: "); Serial.print(rawPowerPrev); 
            Serial.print("power after: "); Serial.println(solarPower); 
            Serial.print("voltage before: "); Serial.print(voltageInputPrev); 
            Serial.print("voltage after: "); Serial.print(rawSolarV);
            Serial.print("pwm: "); Serial.print(duty); 
            Serial.print("delta: "); Serial.println(delta); */
           if(solarPower > rawPowerPrev){
                duty += delta;
                rawPowerPrev = solarPower;
           } else {
                duty -= delta;
                delta /= 2;
                if(delta == 0){                           //  MP MV ; MPP Reached -                                           
                  duty = IIR(storeDuty, duty, 80, 128);   // smooth duty value a bit
                  flip = 1 - flip;
                  mpptReached = 1; // ! indicate MPPT reached
                } 
              }
          }                    
          set_pwm_duty(false);                   // set pwm duty cycle to pwm value
        }
    }
    else if (charger_state == bat_float){  
        int effectiveBound = floatVoltageRaw + tempCompensationRaw - powerCompensation;
        mpptReached = 0;
        if(!finishEqualize && rawBatteryV > floatVoltageRaw + tempCompensationRaw - 25){
          if(absorptionStartTime == 0) absorptionStartTime = currentTime;
          else {
            long interval = currentTime - absorptionStartTime;
            if(interval > 10000) {          
              absorptionAccTime += interval; 
              absorptionStartTime = currentTime;
            }
          }
        } else absorptionStartTime == 0;
        
        if (solarV + 0.2< batteryV) {                         // if watts input from the solar panel is less than
          charger_state = off;                                // the minimum solar watts then it is getting dark so
          off_count = OFF_NUM;                                // go to the charger off state  
          set_pwm_duty(true);         
        }        
        else if (rawBatteryV > effectiveBound) {                    // If we've charged the battery above the float voltage                   
          int delta = (rawBatteryV - (floatVoltageRaw + tempCompensationRaw - powerCompensation)) / 2;
          // Serial.print(batteryV);Serial.print("decrease "); Serial.println(delta);
          duty -= delta;                                      // down
          set_pwm_duty(false);                                     // write the PWM
        }
        else if (rawBatteryV <= effectiveBound) {                    // else if the battery voltage is less than the float voltage - 0.1
          //int delta = 2; // (batteryV - (floatVoltage + tempCompensation - powerCompensation)) / 0.01;
          // Serial.print(batteryV);Serial.print("increase "); Serial.println(delta);
          if(duty < mpptDuty){   // protect duty from drifting up
            duty += 2;                                              // up
            set_pwm_duty(false);                                      
          }
          if (rawBatteryV < floatVoltageRaw + tempCompensationRaw - 40){ //(floatVoltage + tempCompensation - 0.6)) {               // if the voltage drops because of added load,
            if(!finishEqualize && absorptionStartTime > 0){
              absorptionAccTime += currentTime - absorptionStartTime;
              absorptionStartTime = 0;
            }
            charger_state = scanning;                               // switch back into bulk state to keep the voltage up
            flip = 1;
            startTracking = true;
          }
        }
    }      
    else{
        
    }   
}
