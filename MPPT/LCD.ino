#define COM_NUMBER 6                // LCD COM pins
#define MS_PER_COM 2                // LCD COM duty ms
#define INFO_CYCLE 500              // LCD info cycling interval

// LCD
#define EXCLAMATION_COM 4       // !
#define EXCLAMATION_SHIFT 7
#define MPPT_COM 4              // MPPT
#define MPPT_SHIFT 9
#define LINK_COM 3              // Link
#define LINK_SHIFT 9
#define SEALED_COM 5            
#define SEALED_SHIFT 8          // SEALED battery type
#define LIPO_COM 5              
#define LIPO_SHIFT 9            // LiPo battery type

byte
  LCDdigits[3];               // SYSTEM LCD three digits  


void LCDDrive(){ //(unsigned long currentTime){
//  static unsigned long lcdTimestamp = 0;
  static bool 
    flip = false;                      // flip COM from HIGH to LOW or go next
  static byte
    comRight   = 15,                     // LOW or HIGH controled by flip    
    comOutput  = 255,                     // LOW or HIGH controled by flip    
    currentCom  = 0;                       // LCD com pins cycling counter
    
    byte com =  2 + currentCom;
    if(flip){
        // Switch to the next com pin
        bitWrite(DDRD, com, 0); // pinMode(com, INPUT);        // Bias 1/2
        currentCom = (currentCom + 1) % COM_NUMBER;
        com = 2 + currentCom;
        bitWrite(PORTD, com, 1);
        bitWrite(DDRD, com, 1); // pinMode(com, OUTPUT);
        
    } else bitWrite(PORTD, com, 0);
    flip = !flip;    
    comOutput ^= 255; comRight ^= 15;
    PORTB &= ~(1 << PORTB3); // digitalWrite(LP, LOW);
    unsigned int data = LCDmap[currentCom];     
    shiftOut(DP, CP, MSBFIRST, data ^ comOutput); 
    // Set PB3 (pin LPr) HIGH and PB0 (pin LPl) LOW at the same time
    PORTB = (PORTB & ~(1 << PORTB0)) | (1 << PORTB3);
    shiftOut(DP, CP, MSBFIRST, (data >> 8) ^ comRight); // keep 4 MSB intact
    PORTB |= (1 << PORTB0);
}


void Number2Digits(float num, float scale){
  int intPart = (int)(max(num, 0.0) * scale + 0.5); // include first decimal digit rounded
  for(byte i = 0; i < 3; i++){
    LCDdigits[i] = intPart % 10;
    intPart /= 10;
  }  
}

byte
  // digit pos, COM address, bit Shift 
  digitsPos[6][2][7] = {
    {{2,3,2,1,1,0,0}, {3,3,4,4,3,3,4}}, // decimal part right display
    {{2,3,3,2,1,0,1}, {1,1,2,2,1,1,2}},
    {{2,3,2,1,1,0,0}, {6,6,5,5,6,6,5}},
    
    {{2,3,2,1,1,0,0}, {11,11,0,0,11,11,0}}, // decimal part left display
    {{2,3,3,2,1,0,1}, {10,10,7,7,10,10,7}},
    {{2,3,2,1,1,0,0}, {8,8,9,9,8,8,9}}
    },
  // 0 1 2 3 4 5 6 7 8 9 E 
  digits[11] ={0b01110111, 0b01000100, 0b00111110, 0b01101110, 0b01001101, 0b01101011, 0b01111011, 0b01000110, 0b01111111, 0b01101111, 0b00111011};

void PrintOutRight(float num, valueType kind){
    Number2Digits(num, 10.0);
    byte digit = 0;
    byte b = 0;

    for(byte n = 0; n < 3; n++){ // digits cycle
      digit = digits[LCDdigits[n]];
      for(int i = 0; i < 7; i++){ // segments cycle
        b = n == 2 && LCDdigits[n] == 0 ? 0 : bitRead(digit, i);
        bitWrite(
            LCDmap[digitsPos[n][0][i]], 
            digitsPos[n][1][i], 
            b
        );
      }
    }
    bitWrite(LCDmap[0], 2, 1); // dot
    LCDmap[4] &= 0b1111111111100011;
    LCDmap[5] &= 0b1111111111100111; // reset h %
    bitWrite(LCDmap[5], 3, 0); 
    switch(kind){
      case voltage:
        LCDmap[4] |= 0b0000000000001000;
        break;
      case degree:
        LCDmap[4] |= 0b0000000000000100;
        break;      
      case amper:
        LCDmap[4] |= 0b0000000000010000;
        break;      
      case amperHour:
        bitWrite(LCDmap[5], 4, 1); // h
        bitWrite(LCDmap[4], 4, 1); // A
        break;      
      case percent:
        bitWrite(LCDmap[5], 3, 1); // %
        break;      
    }    
}

void PrintOutSolar(float num, valueType kind){        
    Number2Digits(num, kind == power ? 1.0 : 10.0);
    byte digit = 0;
    byte b = 0;

    for(byte n = 0; n < 3; n++){ // digits cycle
      digit = digits[LCDdigits[n]];
      for(int i = 0; i < 7; i++){ // segments cycle
        b = n == 2 && LCDdigits[n] == 0 ? 0 : bitRead(digit, i);
        bitWrite(
            LCDmap[digitsPos[n+3][0][i]], 
            digitsPos[n+3][1][i], 
            b
        );
      }      
    }    
    bitWrite(LCDmap[0], 7, kind == power ? 0 : 1); // dot
    LCDmap[4] &= 0b1111011111111110;
    LCDmap[5] &= 0b1111001111111110;
    switch(kind){
      case voltage:
        bitWrite(LCDmap[4], 11, 1);
        break;   
      case amper:
        bitWrite(LCDmap[4], 0, 1);
        break;      
      case amperHour:
        bitWrite(LCDmap[4], 0, 1); // A
        bitWrite(LCDmap[5], 0, 1); // h
        break;      
      case degree:
        bitWrite(LCDmap[5], 11, 1);
        break;      
    }    
}

void BatteryPercent(){
  float ratio = (batteryV - LVD)/(ABSORPTION_START_V - LVD);
  bitWrite(LCDmap[5], 2, ratio > 0.90 ? 1 : 0);
  bitWrite(LCDmap[5], 1, ratio > 0.70 ? 1 : 0);
  bitWrite(LCDmap[4], 1, ratio > 0.50 ? 1 : 0);
  bitWrite(LCDmap[4], 5, ratio > 0.30 ? 1 : 0);
  bitWrite(LCDmap[5], 5, ratio > 0.10 ? 1 : 0);
  bitWrite(LCDmap[SEALED_COM], SEALED_SHIFT, 1); // SLD
}

void LoadStatus(){
  bitWrite(LCDmap[4], 10, load_status);  
}

void SunStatus(){
  bitWrite(LCDmap[4], 8, !IUV);
}

void ErrorStatus(){
  bitWrite(LCDmap[EXCLAMATION_COM], EXCLAMATION_SHIFT, ERR > 0);
}

void LinkStatus(unsigned long currentTime){
  bitWrite(LCDmap[LINK_COM], LINK_SHIFT, currentTime - lastLinkActiveTime < 3000ul);
}

void PrintMpptStatus(unsigned long currentTime){
    static unsigned long lastBlink    = 0;
    static byte          blinkState   = HIGH;
      switch(charger_state){
        case bat_float:
          // indicate battery is in float state
          if(currentTime - lastBlink > 500){
            blinkState = !blinkState;
            lastBlink = currentTime;
          }          
          bitWrite(LCDmap[MPPT_COM], MPPT_SHIFT, blinkState);
          break;
        case bulk:
          bitWrite(LCDmap[MPPT_COM], MPPT_SHIFT, mpptReached); 
          break;
        default:          
          break;
      }
}

void LCDinfo(unsigned long currentTime){
  static unsigned long lcdInfoTimestamp = 0;           // Last info switch timestamp
  static byte          
    test = 0,
    innerCycle       = 0,
    LCDinfoCycle     = 0;           // cycling info on the screen counter
  
      PrintMpptStatus(currentTime);
      if(currentTime - lcdInfoTimestamp > INFO_CYCLE){
        BatteryPercent(); 
        LoadStatus();
        SunStatus();
        ErrorStatus();  
        LinkStatus(currentTime);
        bitWrite(LCDmap[LIPO_COM], LIPO_SHIFT, LCDcycling); // LiPo     
        if(LCDcycling && !(innerCycle++ % 8)) LCDinfoCycle = (LCDinfoCycle + 1) % 5;        
        switch(LCDinfoCycle){
          case 0:
            PrintOutRight(batteryV, voltage);
            PrintOutSolar(solarV, voltage);
            break;
          case 1:
            PrintOutRight(temperature, degree);
            PrintOutSolar(Voltage2Temp(BTS), degree);
            break;
          case 2:            
            PrintOutRight(currentLoad, amper);
            PrintOutSolar(currentInput, amper);
            break;
          case 3:
            PrintOutRight(duty/10.23, percent);
            PrintOutSolar(sol_watts, power);
            break;
          case 4:
            PrintOutRight(todayOutAh, amperHour);
            PrintOutSolar(todayAh, amperHour);
            break;
        }
        lcdInfoTimestamp = currentTime;
      }
      // LCDDrive(currentTime);      
    }
    
ISR(TIMER2_COMPA_vect){//timer1 interrupt 30Hz
  LCDDrive();
}
