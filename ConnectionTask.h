#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "Wifi.h"
#include "MQTT.h"
#include "DataTransform.h"
#include "files.h"
#include "esp_task_wdt.h"

class ConnectionTask {
  private:
    typedef void (*ResetReason)(char* reason);
    ResetReason resetReason;
    char 
              MAC[18], 
             SSID[32], 
              PWD[64], 
               Id[50], 
         Password[30], 
      MQTT_Server[50];
      
    uint16_t MQTT_Port, currentTime;
    uint8_t Mode;
    MQTT mqtt;
    
    Ticker mainTicker;
    
    bool mqttConnected = false, firstTime = true, firstCertificate = true, server_error = false;
    void sendCommand(const char* command, const char* payload);
    void setWDT(bool value);
  public:
    Wifi wifi;
    ConnectionTask(QueueHandle_t sender, QueueHandle_t receiver);
    bool doMainTimer = false;
    void init();
    void MainTimer();
    void setResetReason(ResetReason interface);
    void wifiConnection(bool isConnected, const char* reason);
    void WifiDataReceived(char* command, uint8_t* payload, uint16_t size);
    void MQTTConnected(bool isConnected);
    void MQTTReceived(char* topic, uint8_t* payload, uint16_t length);    
    void DataTransformListener(char* command, char* payload);
    void loop();
};
