#define INVERTOR_RELAY PB1
#define CHARGER_RELAY PB0
#define VOLTAGE_SENSOR PB2 
#define INVERTOR_ON PB3 

// 13.25V
#define INVERTOR_ON_THR 9135
// 12.15V
#define INVERTOR_OFF_THR 8350
// 11.73V
#define CHARGER_ON_THR 8050
// 12.6V
#define CHARGER_OFF_THR 8700
// 5 minutes
//#define ON_INVERTOR_MIN_TIME 5000L
#define ON_INVERTOR_MIN_TIME 300000L
// 5 minutes
#define SWITCHING_DELAY 5000L
// 30 minutes
#define OFF_INVERTOR_MIN_TIME 1500000L


//unsigned long estimatorStart = 0;
unsigned long onInvertorStart = 0;
unsigned long offInvertorStart = 0;
bool invertorStarted = false;
bool switchingDelay = false;
bool switchingOffDelay = false;
unsigned long delayingStart = 0;
char voltageLowCounter = 0;
//bool inEstimateMode = false;


void setup() {
  analogReference(INTERNAL); // switch to 1.1volt Aref
  pinMode(INVERTOR_RELAY, OUTPUT);
  pinMode(INVERTOR_ON, OUTPUT);
  pinMode(CHARGER_RELAY, OUTPUT);
  pinMode(VOLTAGE_SENSOR, INPUT);
//  invertorStarted=true;
//  digitalWrite(INVERTOR_ON, HIGH);
}
 
// the loop function runs over and over again forever
void loop() {    
    int invertorOnThr ;
    int voltage = 0;
    unsigned long currentTime = millis();

    for(int i = 0; i < 10; i++) {
      delay(300);
      voltage += analogRead(VOLTAGE_SENSOR-1); // -1 ATtiny chip specifics
    }

    if(voltage < CHARGER_ON_THR){
      PORTB |= _BV(CHARGER_RELAY);
      // digitalWrite(CHARGER_RELAY, HIGH);
    } else if(voltage > CHARGER_OFF_THR){
      PORTB &= ~_BV(CHARGER_RELAY);
      //estimatorStart = min(currentTime, estimatorStart);
      //inEstimateMode = true;
      // int deltaV = CHARGER_OFF_THR - voltage; // voltage grow
      //int deltaT = (currentTime - estimatorStart) / (31L * (CHARGER_OFF_THR - voltage)); // minutes went      
      //invertorOnThr = deltaT  + CHARGER_OFF_THR;
      
      // digitalWrite(CHARGER_RELAY, LOW);
    } else   {invertorOnThr = INVERTOR_ON_THR;}
    
 /*   for(int i = 0; i < (voltage-8500) / 100;i++) {
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
        // 5 sec delay before power on to prevent traveling arc between relay contacts
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
        // 5 sec delay before power on to prevent traveling arc between relay contacts
          PORTB &= ~_BV(INVERTOR_RELAY);
          // digitalWrite(INVERTOR_RELAY, LOW);
          offInvertorStart = currentTime;
          invertorStarted = false;
          switchingOffDelay = false;
        }                 
    }
    
}
