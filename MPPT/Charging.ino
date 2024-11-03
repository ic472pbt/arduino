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

//  Float State - As the battery charges it's voltage rises. When it gets to the MAX_BAT_VOLTS we are done with the 
//      bulk battery charging and enter the battery float state. In this state we try and keep the battery voltage
//      at MAX_BAT_VOLTS by adjusting the pwm value. If we get to pwm = 100% it means we can't keep the battery 
//      voltage at MAX_BAT_VOLTS which probably means the battery is being drawn down by some load so we need to back
//      into the bulk charging mode.
IState* floatState::Handle(float PVvoltage, unsigned long currentTime) {
        IState* newState = this;
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
        
        if (PVvoltage + 0.2< batteryV) {                      // if watts input from the solar panel is less than
          newState = offInstance;                             // the minimum solar watts then it is getting dark so
          off_count = OFF_NUM;                                // go to the charger off state  
          set_pwm_duty(true);         
        }        
        else if (rawBatteryV > effectiveBound) {                    // If we've charged the battery above the float voltage                   
          int delta = (rawBatteryV - (floatVoltageRaw + tempCompensationRaw - powerCompensation)) / 2 + stepsDown * 2;
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
            newState = scanInstance;                               // switch back into bulk state to keep the voltage up
            this->dirrection = 1;
            startTracking = true;
          }
        }
        return newState;
};

//  Off State - This is state that the charger enters when solar watts < MIN_SOL_WATTS. The charger goes into this
//      off in this state so that power from the battery doesn't leak back into the solar panel. 
class offState : public IState {
  public:
    offState() : IState("off") {}
    
    IState* Handle(float PVvoltage, unsigned long currentTime) override {
      IState* newState = this;
      bool solarOff = true;
      int floatV = floatVoltageRaw + tempCompensationRaw;

      mpptReached = 0;
      if (off_count > 0) {                                  // this means that we run through the off state OFF_NUM of times with out doing
          off_count--;                                        // anything, this is to allow the battery voltage to settle down to see if the  
      }                                                     // battery has been disconnected
      else if ((rawBatteryV > floatV) && (PVvoltage > batteryV)) {
          newState = floatInstance;                         // if battery voltage is still high and solar volts are high
          solarOff = false;
      }    
      else if ((batteryV > LVD) && (rawBatteryV < floatV) && (PVvoltage > batteryV)) {
          newState = scanInstance;
          solarOff = false;
          duty = MIN_ACTIVE_DUTY;
      }
      set_pwm_duty(solarOff);  
      return newState;
    }
};

//  Bulk State - this is charger state for solar watts > MIN_SOL_WATTS. This is where we do the bulk of the battery
//      charging and where we run the Peak Power Tracking alogorithm. In this state we try and run the maximum amount
//      of current that the solar panels are generating into the battery.
class bulkState : public IState {
  private:
    unsigned long 
      lastTrackingTime,
      rawPowerPrev   = 0;
    byte 
      stepSize = MAX_PWM_DELTA;
    int 
      delta = 10,      
      voltageInputPrev = 0;
  public:
    bulkState() : IState("bulk") {}

    IState* Handle(float PVvoltage, unsigned long currentTime) override {
      IState* newState = this;
      if (solarV + 0.2 < batteryV) {                        // if watts input from the solar panel is less than          
          newState = offInstance;                           // the minimum solar watts then it is getting dark so
          off_count = OFF_NUM;                              // go to the charger off state
          set_pwm_duty(true); 
      }
      else if (rawBatteryV > floatVoltageRaw + tempCompensationRaw - powerCompensation){
        mpptDuty = duty;
        controlFloat = true;                                // else if the battery voltage has gotten above the float
        currentState = floatInstance;                        // battery float voltage go to the charger battery float state            
      }                          
      else if (sol_watts < LOW_SOL_WATTS) {                 // else if the solar input watts is less than low solar watts
          newState = onInstance;                            // it means there is not much power being generated by the solar panel
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
              delta = floatInstance->dirrection * stepSize;
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
                  // duty -= stepsDown * 4;               // protection against repeated overpower events                                            
                  duty = IIR(storeDuty, duty, 80, 128);   // smooth duty value a bit
                  floatInstance->Reverse();               // Change the direction for the next tracking.
                  mpptReached = 1; // ! indicate MPPT reached
                } 
              }
          }                    
          set_pwm_duty(false);                   // set pwm duty cycle to pwm value
        }
        return newState;      
    }
};

// scanState - This is the charger state used for scanning solar power levels to find the optimal duty cycle.
//      When solar watts are above MIN_SOL_WATTS, the charger enters this state to maximize the charging efficiency
//      by finding the peak power point. In this state, the charger tests different duty cycles and tracks
//      the power generated by the solar panels, aiming to identify the best duty cycle that results in maximum power.
//      Once the best power point is identified, the charger switches to the bulk charging state.
class scanState : public IState {
  private:
    byte cycleNum = 0;
    unsigned long bestPower = 0;
    unsigned int bestDuty = 0; 
  public:
    scanState() : IState("scan") {}
    
    IState* Handle(float PVvoltage, unsigned long currentTime) override {
        IState* newState = this;        
        if(cycleNum == 10){
          cycleNum = 0;
          bestPower = 0;
          duty = bestDuty;
          newState = bulkInstance;
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
        return newState;   
    }
};

//  On State - this is charger state for MIN_SOL_WATTS < solar watts < LOW_SOL_WATTS. In this state is the solar 
//      watts input is too low for the bulk charging state but not low enough to go into the off state. 
//      In this state we just set the pwm = 99.9% to get the most of low amount of power available.
class onState : public IState {
  public:
    onState() : IState("on") {}
    
    IState* Handle(float PVvoltage, unsigned long currentTime) override {
        IState* newState = this;
        mpptReached = 0;
        if (solarV + 0.2 < batteryV) {                      // if watts input from the solar panel is less than
          newState = offInstance;                           // the minimum solar watts then 
          off_count = OFF_NUM;                              // go to the charger off state
          set_pwm_duty(true); 
          /* Serial.print(solarV); Serial.print(' ');
          Serial.print(batteryV);
          Serial.println("go off");*/
        }
        else if (rawBatteryV > (BATT_FLOAT_RAW - 7)) {            // else if the battery voltage has gotten above the float
          newState = floatInstance;                               // battery float voltage go to the charger battery float state
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
          newState = scanInstance;
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
        return newState;
    }   
};

//------------------------------------------------------------------------------------------------------
// This routine is the charger state machine. It has four states on, off, bulk and float.
// It's called once each time through the main loop to see what state the charger should be in.
// The battery charger can be in one of the following five states:
// 
IState* offInstance = new offState();
floatState* floatInstance = new floatState();
IState* bulkInstance = new bulkState();
IState* scanInstance = new scanState();
IState* onInstance = new onState();
  
//------------------------------------------------------------------------------------------------------
void Charging_Algorithm(float sol_volts, unsigned long currentTime) { 
  // rawPowerOld        = 0;    // solar watts from previous time through ppt routine 
  
  if(ERR>0){
    currentState = offInstance;
    duty-=100;
    set_pwm_duty(true);
    return;
  }else if (REC==1){ // wait for recovery from low solar voltage (starting a new day)
      REC=0;
//      while(ADS.isBusy()){}
//      inCurrentOffset = ADS.readADC(CURRENT_IN_SENSOR) + 2;   // zero current offset
//      ADS.requestADC(currentADCpin);    
      duty = MIN_ACTIVE_DUTY;  
      currentState = floatInstance;
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

    IState* newState = currentState->Handle(sol_volts, currentTime);
    if (newState != currentState) {
      currentState = newState; // Update to the new state if there's a transition
    }
     
}
