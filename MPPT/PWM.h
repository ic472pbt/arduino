#ifndef PWM_H
#define PWM_H

#define PWM_PIN 9 // PWM pin
#define MIN_ACTIVE_DUTY 300              // minimum duty sensitive to current change
#define MAX_DUTY 1023                    // maximum duty

#include "IIRFilter.h"
#include <TimerOne.h>

/* The PWM class manages pulse-width modulation (PWM) for controlling a device connected to 
 * a specific PWM pin (defined as pin 9). This class relies on the TimerOne library to set the PWM 
 * duty cycle and provides methods for turning PWM on and off, as well as for limiting the PWM duty 
 * cycle within specified bounds.
*/
class PWM {  
  private:
    unsigned int
      storedDuty;
    IIRFilter& 
      filter;  // Reference to an IIRFilter instance

    
  public:
    unsigned int 
      mpptDuty,                  // store pwm duty at mppt point to limit in float mode
      duty;                      // pwm duty    

    // Constructor that accepts an IIRFilter instance
    PWM(IIRFilter& filterInstance) : filter(filterInstance), mpptDuty(0), duty(0) {}
        
    void shutdown(){
      storedDuty = duty;
      setDuty();
    }

    void resume() {
      setDuty(storedDuty);
    }
    
    void slowResume() {
      setDuty(storedDuty/2);
    }

    void setDuty(unsigned int D = 0) {
        duty = min(1023, D);                             
        Timer1.pwm(PWM_PIN, duty);
    }
    
    void initIIR(){
      filter.reset(duty);  
    }
    
    void setMinDuty(){
      setDuty(MIN_ACTIVE_DUTY);
    }

    void smoothDuty(){
      setDuty(filter.smooth(duty));  
    }

    void storeMpptDuty(){
      mpptDuty = duty;  
    }
    
    void incrementDuty(int delta){
      setDuty(duty + delta);
    }

    // Initialization function to set up Timer1 settings
    void initialize(int T) {
        Timer1.initialize(T);     // Initialize Timer1 with a 40 µs period (~25 kHz)
        // Timer1.pwm(PWM_PIN, 0);   // Set initial PWM duty cycle to 0 on the PWM pin
        shutdown();               
    }

    bool isShuteddown() {
      return duty == 0;
    }
};
#endif // PWM_H
