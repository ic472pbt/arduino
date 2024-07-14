// 14/07/2024 to be deployed
#define INVERTOR_RELAY PB1
#define CHARGER_RELAY PB0
#define VOLTAGE_SENSOR PB2 
#define INVERTOR_ON PB3 

// 13.7V <- 13.6
#define INVERTOR_ON_THR 9411
// 12.1V <- 12.0
#define INVERTOR_OFF_THR 8318
#define CHARGER_ON_THR 8550  // 12.5V
#define CHARGER_OFF_THR 8950 // 13.0V
// 5 minutes
#define ON_INVERTOR_MIN_TIME 300000L
// 120 seconds
#define SWITCHING_DELAY 120000L
// 30 minutes
#define OFF_INVERTOR_MIN_TIME 1500000L


//unsigned long estimatorStart = 0;
unsigned long 
  onInvertorStart  = 0,
  offInvertorStart = 0,
  delayingStart = 0;
bool 
  invertorStarted = false,
  switchingDelay  = false,
  switchingOffDelay = false;
char voltageLowCounter = 0;


void setup() {
  analogReference(INTERNAL); // switch to 1.1volt Aref
  pinMode(INVERTOR_RELAY, OUTPUT);
  pinMode(INVERTOR_ON, OUTPUT);
  pinMode(CHARGER_RELAY, OUTPUT);
  pinMode(VOLTAGE_SENSOR, INPUT);
//  invertorStarted=true;
//  digitalWrite(INVERTOR_ON, HIGH);
}

unsigned int IIR(unsigned int oldValue, unsigned int newValue){
  return (unsigned int)(((unsigned long)(oldValue) * 90ul + (unsigned long)(newValue) * 10ul) / 100ul);
}
 
// the loop function runs over and over again forever
void loop() {    
    static int voltage = analogRead(VOLTAGE_SENSOR-1);  // -1 ATtiny chip specifics
    int invertorOnThr ;
    
    unsigned long currentTime = millis();

    voltage = IIR(voltage, analogRead(VOLTAGE_SENSOR-1));

    if(voltage < CHARGER_ON_THR){
      PORTB |= _BV(CHARGER_RELAY);
      // digitalWrite(CHARGER_RELAY, HIGH);
    } else if(voltage > CHARGER_OFF_THR){
      PORTB &= ~_BV(CHARGER_RELAY);
    } else   {invertorOnThr = INVERTOR_ON_THR;}
    
 /*   for(int i = 0; i < (voltage-8500) / 100;i++) { // test ADC
      delay(500);
      digitalWrite(CHARGER_RELAY, HIGH);
      delay(500);
      digitalWrite(CHARGER_RELAY, LOW);
    }*/

    // mode switch latch
    voltageLowCounter += 
      invertorStarted && (voltage < INVERTOR_OFF_THR) && (currentTime - onInvertorStart > ON_INVERTOR_MIN_TIME) ? 1 : 
      (!invertorStarted && (voltage > INVERTOR_ON_THR) && ((currentTime - offInvertorStart > OFF_INVERTOR_MIN_TIME) || (offInvertorStart == 0)) ? -1 : 
      -voltageLowCounter);
    
    if(switchingDelay || voltageLowCounter < -2){
        switchingOffDelay = false;
        PORTB |= _BV(INVERTOR_RELAY);
        PORTB &= ~_BV(CHARGER_RELAY);
        // digitalWrite(INVERTOR_RELAY, HIGH);
        // digitalWrite(CHARGER_RELAY, LOW);      
        if(!switchingDelay) {
          delayingStart = currentTime;
          switchingDelay = true;
        }else if(currentTime - delayingStart > SWITCHING_DELAY){
        // 2 mins delay before power on to prevent traveling arc between relay contacts
          PORTB |= _BV(INVERTOR_ON);
          // digitalWrite(INVERTOR_ON, HIGH);
          onInvertorStart = currentTime;
          invertorStarted = true;
          switchingDelay = false;
        }                     
    }
  
    else if(switchingOffDelay || voltageLowCounter > 2){        
        switchingDelay = false;
        PORTB &= ~_BV(INVERTOR_ON);
        // digitalWrite(INVERTOR_ON, LOW);
        if(!switchingOffDelay) {
          delayingStart = currentTime;
          switchingOffDelay = true;
        }else if(currentTime - delayingStart > SWITCHING_DELAY){
        // 2 mins delay before power on to prevent traveling arc between relay contacts
          PORTB &= ~_BV(INVERTOR_RELAY);
          // digitalWrite(INVERTOR_RELAY, LOW);
          offInvertorStart = currentTime;
          invertorStarted = false;
          switchingOffDelay = invertorStarted;
        }                 
    }
}
