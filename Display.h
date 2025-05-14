#ifndef DISPLAY_h
#define DISPLAY_h

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

class Display {
  private:
    Adafruit_SSD1306 display;
    

  public:
               Display() {      display.begin(SSD1306_SWITCHCAPVCC, 0x3D);    }
    void dim(bool value) {      display.dim(value);                           }
    
    /**
    * show the humidity and temperature on the OLED display
    */
    void showInformation(uint8_t moisture, float temperature) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.printf("%d%% , %.1f\n", moisture, temperature);
      display.display();
      display.dim(false);
    }
};

#endif