#ifndef BUTTON_h
#define BUTTON_h

#include <Arduino.h>

class Button {
  private:
    typedef void (*TouchListener)(uint8_t state);
    uint8_t pin, pressValue, lastReading, step = 1;
    bool changeState;
    uint32_t lastDebounceTime, lastTime;
    
    TouchListener touchListener;
  public:

    /**
    * Initialize class
    */
    Button() {
      touchListener = NULL;
      //According to the PCB layout press value can be LOW or HIGH
      pressValue    = LOW;
    }

    /**
    * Initialization class with arguments
    */    
    Button(uint8_t active, uint8_t pin) {
      pressValue = active; //ActiveLow or ActiveHigh is determined
      pin        = pin;    //Push button GPIO is set here
      init();
    }

    /**
    * start of object
    */
    void init() {
      lastTime         = 0; // variables to check debounce
      lastDebounceTime = 0;

      if (pressValue == LOW)
        pinMode(pin, INPUT_PULLUP); //Needs to be pulled up when active is LOW

      lastReading = digitalRead(pin);
    }

    /**
    * Set Button instance parameters
    */
    void setParameters(uint8_t active, uint8_t pin) {
      pressValue = active;
      pin        = pin;
      init();
    }

    /**
    * Sets callback for push button's states
    */
    void setOnTouchListener(TouchListener listener) {  touchListener = listener;    }

    /**
    * Handles the push button jobs and event
    */
    void loop() {
      uint8_t newReading = digitalRead(pin); // Check if the push button's state is changed or not
      if (newReading != lastReading) {
        lastDebounceTime = millis(); //Set the time stamp to do debounce job
        changeState = true;
      }

      if (changeState)
        if ((millis() - lastDebounceTime) > 50) { // If debounce time of 50 milliseconds is passed then..
          changeState = false;
          lastTime = millis();
          if (touchListener != NULL)
            if (newReading == pressValue) {
              step = 1;
              touchListener(1); //1 means the push button is pressed
            }
            
            else if (step == 1) { // if push button is pressed previously and now is released
              step = 0;
              touchListener(2); //2 means push button is released
            }
        }

      //If push button is pressed for at least 3 seconds
      if (step == 1 && (millis() - lastTime >= 3000)) {
        step = 0;
        if (touchListener != NULL)
          touchListener(3); //3 means long press
      }
      lastReading = newReading;
    }
};

#endif