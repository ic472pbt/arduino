
#define LVR 12.6
#define HVD 15.8
#define LOAD_DISCONNECT_LIMIT 60000  // USER PARAMETER - Time interval for load disconnection on error

bool 
 load_status = true;          // variable for storing the load output state
 
void load_on(boolean new_status) {
  if (load_status != new_status) {
    if(new_status) {
      digitalWrite(LOAD, LOW);
      Serial.println("load on");
    }
    else {
      digitalWrite(LOAD, HIGH);
      Serial.println("load off");
    }
    load_status = new_status;    
  }
}

void Device_Protection(unsigned long currentTime, float voltageOutput){
  static unsigned long
    lvdLastTime        = 0,
    lastLoadDisconnect = 0;
  static bool 
    prev_lvd_event = false,          //low voltage disconnect event
    lvd_event = false,               //low voltage disconnect event
    hvd_event = false;               //high voltage disconnect event         
    
  //FAULT DETECTION     
  ERR = OTE || IOC || IUV || BNC;                                                                                          //Reset local error counter
    
  if(sensors.values.batteryV>HVD)                                      {OOV=1;ERR++;hvd_event=true;}else{OOV=0;}  //OOV - OUTPUT OVERVOLTAGE: Output voltage has reached absolute limit                     
  if(sensors.values.batteryV<LVD){
    if(!prev_lvd_event){
      prev_lvd_event=true;
      lvdLastTime = currentTime;
    }else if(currentTime - lvdLastTime > 5000L){ // 5 seconds delay
      FLV=1;ERR++;
      lvd_event = prev_lvd_event;
    }
  }else{FLV=0; prev_lvd_event = false;}  //FLV - Fatally low system voltage (unable to resume operation)

  // Load protection
  if((OOV || FLV || OOC) && load_status){
    // disconnect the load in case of deep discharge or overcharge
    load_on(false);
    lastLoadDisconnect = currentTime;
    StoreHarvestingData(currentTime);
  }else if(currentTime - lastLoadDisconnect > LOAD_DISCONNECT_LIMIT){
    lvd_event &= (sensors.values.batteryV <= LVR);
    hvd_event &= (sensors.values.batteryV >= HVD);

    load_on(!(lvd_event || hvd_event)); // also OOC recovery
  }   
}
