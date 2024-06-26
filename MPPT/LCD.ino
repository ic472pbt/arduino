#define COM_NUMBER 6                // LCD COM pins
#define MS_PER_COM 2                // LCD COM duty ms
#define INFO_CYCLE 500              // LCD info cycling interval


byte
  LCDdigits[3];               // SYSTEM LCD three digits  


void LCDDrive(){ //(unsigned long currentTime){
//  static unsigned long lcdTimestamp = 0;
  static bool 
    flip = false;                      // flip COM from HIGH to LOW or go next
  static byte
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
    comOutput ^= 255;
    // digitalWrite(com, comOutput);
      
      /* draw solar V good
      byte solarVisGood = IUV == 0 ? HIGH : LOW;
      digitalWrite(SEG1, solarVisGood ^ com5);

      // draw load is ON
      byte loadIsOn = load_status ? HIGH : LOW;
      digitalWrite(SEG3, loadIsOn ^ com5);     */  


    PORTB &= ~(1 << PORTB3); // digitalWrite(LP, LOW);
    shiftOut(DP, CP, MSBFIRST,  LCDmap[currentCom] ^ comOutput); 
    PORTB |= (1 << PORTB3);  // digitalWrite(LP, HIGH);      
}


void Number2Digits(float num){
  int intPart = (int)(num * 10.0 + 0.5); // include first decimal digit rounded
  for(byte i = 0; i < 3; i++){
    LCDdigits[i] = intPart % 10;
    intPart /= 10;
  }  
}

byte
  // digit pos, COM address, bit Shift 
  digitsPos[3][2][8] = {
    {{2,3,2,1,1,0,0,0}, {3,3,4,4,3,3,4,7}}, // decimal part
    {{2,3,3,2,1,0,1,0}, {1,1,2,2,1,1,2,7}},
    {{2,3,2,1,1,0,0,0}, {6,6,5,5,6,6,5,7}}
    },
  digits[10] ={0b01110111, 0b01000100, 0b00111110, 0b01101110, 0b01001101, 0b01101011, 0b01111011, 0b01000110, 0b01111111, 0b01101111};
void PrintOutRight(float num, valueType kind){
    Number2Digits(num);
    byte digit = 0;
    byte b = 0;

    for(byte n = 0; n < 3; n++){ // digits cycle
      digit = digits[LCDdigits[n]];
      for(int i = 0; i < 8; i++){ // segments cycle
        b = bitRead(digit, i);
        bitWrite(
            LCDmap[digitsPos[n][0][i]], 
            digitsPos[n][1][i], 
            b
        );
      }      
    }
    bitWrite(LCDmap[0], 2, 1); // dot
    LCDmap[4] &= 0b11100011;
    bitWrite(LCDmap[5], 3, 0); // %
    switch(kind){
      case voltage:
        LCDmap[4] |= 0b00001000;
        break;
      case degree:
        LCDmap[4] |= 0b00000100;
        break;      
      case amper:
        LCDmap[4] |= 0b00010000;
        break;      
      case percent:
        bitWrite(LCDmap[5], 3, 1);
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
  bitWrite(LCDmap[5], 6, 1); // GEL
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
          bitWrite(LCDmap[EXCLAMATION_COM], EXCLAMATION_SHIFT, blinkState);
          break;
        case bulk:
          bitWrite(LCDmap[EXCLAMATION_COM], EXCLAMATION_SHIFT, mpptReached); 
          break;
        default:          
          break;
      }
}

void LCDinfo(unsigned long currentTime){
  static unsigned long lcdInfoTimestamp = 0;           // Last info switch timestamp
  static byte          
    innerCycle       = 0,
    LCDinfoCycle     = 0;           // cycling info on the screen counter
  
      PrintMpptStatus(currentTime);
      if(currentTime - lcdInfoTimestamp > INFO_CYCLE){
        BatteryPercent(); 
                
        if(innerCycle++ % 6 == 0) LCDinfoCycle = (LCDinfoCycle + 1) % 6;        
        switch(LCDinfoCycle){
          case 0:
            PrintOutRight(batteryV, voltage);
            break;
          case 1:
            PrintOutRight(solarV, voltage);
            break;
          case 2:
            PrintOutRight(temperature, degree);
            break;
          case 3:
            PrintOutRight(Voltage2Temp(BTS), degree);
            break;
          case 4:
            PrintOutRight(currentInput, amper);
            break;
          case 5:
            PrintOutRight(duty/10.23, percent);
            break;
        }
        lcdInfoTimestamp = currentTime;
      }
      // LCDDrive(currentTime);      
    }
    
ISR(TIMER2_COMPA_vect){//timer1 interrupt 30Hz
  LCDDrive();
}
