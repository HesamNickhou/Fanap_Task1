#include <Arduino.h>
#include "TaskDataTransformer.h"
#include "Display.h"
#include "Button.h"
#include "FileHelper.h"
#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

class MainTask {
  private:
    TaskDataTransformer dataTransform;
    OneWire oneWire;
    DallasTemperature ds18b20;
    Display display;
    Ticker ticker1, ticker2;
    Button button;
    FileHelper file;

    struct TSetting {
      char 
                 SSID[32],
                  PWD[64],
              MQTT_Id[50],
        MQTT_Password[40],
          MQTT_Server[50];
      uint8_t Mode;
      uint16_t MQTT_Port, readingInterval;
    } setting;

    uint8_t  moisture, moisturePin;
    float    temperature;
    uint32_t timeStamp;
    void readConfig();
    void saveConfig();
  public:
    MainTask(QueueHandle_t sender, QueueHandle_t receiver);
    void Initialize();
    void DataTransformListener(char* command, char* payload);
    void ButtonListener(uint8_t state);
    void Loop();
};