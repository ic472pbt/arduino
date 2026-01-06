// 04/08/2024 to be deployed
#define INVERTOR_RELAY PB1
#define CHARGER_RELAY PB0
#define VOLTAGE_SENSOR PB2 
#define INVERTOR_ON PB3 

#define LOW_BAT_THR 802      // 11.7V
#define INVERTOR_ON_THR 910  // 13.5V 
#define INVERTOR_OFF_THR 830 // 12.2V 
#define CHARGER_ON_THR 850   // 12.4V good
#define CHARGER_OFF_THR 875  // 13.0V good
// 20 minutes
#define ON_INVERTOR_MIN_TIME 4700 
// 60 seconds
#define SWITCHING_DELAY 235 // 60000 //  
// 15 minutes
#define OFF_INVERTOR_MIN_TIME 3500 // 900000 //  


unsigned int 
  voltage  = 0,
  lastRun  = 0,
  onInvertorStart  = 0,
  offInvertorStart = 0,
  delayingStart = 0;
bool 
  invertorStarted = false,
  switchingDelay  = false,
  switchingOffDelay = false;


/* ==== ADC READ ==== */
static inline uint16_t adc_read(void) {
  ADCSRA |= (1<<ADSC);
  while (ADCSRA & (1<<ADSC));
  return ADC;
}

/* ==== SETUP ==== */
void setup(void) {
  DDRB = _BV(INVERTOR_RELAY) | _BV(CHARGER_RELAY) | _BV(INVERTOR_ON);
  PORTB = 0;

  ADMUX  = (1<<REFS0) | 1;      // 1.1V ref, ADC1
  ADCSRA = (1<<ADEN) | 6;       // enable, clk/64

  voltage = adc_read();
}

// the loop function runs over and over again forever
void loop() {  
    static byte lowVoltageCounter = 0;  
    static byte dischargeVoltageCounter = 0;  
    unsigned long currentTime = millis();
    // unsigned int curTime = (unsigned int)(currentTime >> 8 & 0xFF); // 256ms resolution at 65536 ~ 4.66 h
    byte *p = (byte *)&currentTime; // pointer to the bytes of the value
    // Get the two middle bytes for the little-endian
    unsigned int curTime = ((unsigned int)p[2] << 8) | p[1];
 /*     for(int i = 1; i <= (voltage-900) / 10;i++) { // test ADC
        delay(500);
        digitalWrite(CHARGER_RELAY, HIGH);
        delay(500);
        digitalWrite(CHARGER_RELAY, LOW);
      } 
      delay(10000);
  */  

    if(curTime - lastRun >= 4){ // 1024ms read intervals
      lastRun = curTime;
      voltage = (voltage * 19u + adc_read()) / 20u; // averaging by 95/5 ratio
      lowVoltageCounter += voltage < LOW_BAT_THR ? 1 : -lowVoltageCounter;
      dischargeVoltageCounter += voltage < INVERTOR_OFF_THR ? 1 : -dischargeVoltageCounter;
        
          if(voltage < CHARGER_ON_THR) PORTB |= _BV(CHARGER_RELAY); // digitalWrite(CHARGER_RELAY, HIGH);      
      else 
          if(voltage > CHARGER_OFF_THR) PORTB &= ~_BV(CHARGER_RELAY);
      
      if(invertorStarted){
        if(switchingOffDelay || lowVoltageCounter > 5 || ((voltage < INVERTOR_OFF_THR) && (dischargeVoltageCounter > 30) && (curTime - onInvertorStart >= ON_INVERTOR_MIN_TIME))){        
          switchingDelay = false;
          PORTB &= ~_BV(INVERTOR_ON);         // digitalWrite(INVERTOR_ON, LOW);
          if(!switchingOffDelay) {
            delayingStart = curTime;
            switchingOffDelay = true;
          }else 
            // 2 mins delay before power on to prevent traveling arc between relay contacts
            if(curTime - delayingStart > SWITCHING_DELAY){
              PORTB &= ~_BV(INVERTOR_RELAY);  // digitalWrite(INVERTOR_RELAY, LOW);
              offInvertorStart = curTime;
              invertorStarted = false;
              switchingOffDelay = invertorStarted;
            }                 
        }
      }else{        
        if(switchingDelay || (       
              (voltage > INVERTOR_ON_THR) && 
              ((curTime - offInvertorStart >= OFF_INVERTOR_MIN_TIME) || (offInvertorStart == 0)) // after the min shutdown time or on power on event
            ) 
          ){
            switchingOffDelay = false;
            PORTB |= _BV(INVERTOR_RELAY);   // digitalWrite(INVERTOR_RELAY, HIGH);   
            if(!switchingDelay) {
              delayingStart = curTime;
              switchingDelay = true;
            }else 
              if(curTime - delayingStart > SWITCHING_DELAY){
                // 2 mins delay before power on to prevent traveling arc between relay contacts
                PORTB |= _BV(INVERTOR_ON);  // digitalWrite(INVERTOR_ON, HIGH);
                onInvertorStart = curTime;
                invertorStarted = true;
                switchingDelay = false;
              }                     
        }
    }
  }
}
