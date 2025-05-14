#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "WiFi.h"
#include "MQTT.h"
#include "TaskDataTransformer.h"
#include "FileHelper.h"
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
      
    uint16_t MQTT_Port;
    uint8_t Mode;
    MQTT mqtt;
    TaskDataTransformer dataTransform;
    
    
    bool mqttConnected = false, firstCertificate = true, server_error = false;
    void sendCommand(const char* command, const char* payload);
  public:
    Wifi wifi;
    ConnectionTask(QueueHandle_t sender, QueueHandle_t receiver);
    void wifiConnection(bool isConnected);
    void WifiDataReceived(char* command, uint8_t* payload, uint16_t size);
    void MQTTConnected(bool isConnected);
    void MQTTReceived(char* topic, uint8_t* payload, uint16_t length);    
    void DataTransformListener(char* command, char* payload);
    void loop();
};
