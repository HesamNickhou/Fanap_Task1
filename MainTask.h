#include <Arduino.h>
#include "DataTransform.h"
#include "Display.h"
#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class MainTask {
  private:
    TaskDataTransform dataTransform;
    OneWire oneWire;
    DallasTemperature ds18b20;
    Display display;
    Ticker ticker1, ticker2;
    uint8_t  moisture, moisturePin;
    uint16_t readingInterval;
    float    temperature;
    uint32_t timeStamp;
    void readConfig();
    void saveConfig();
  public:
    MainTask(QueueHandle_t sender, QueueHandle_t receiver);
    void Initialize();
    void DataTransformListener(char* command, char* payload);
    void Loop();
}