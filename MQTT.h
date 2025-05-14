#ifndef MQTT_h
#define MQTT_h

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "FS.h"
#include "SPIFFS.h"

class MQTT {
  private:
    typedef void (*ConnectionListener)(bool isConnected);
    typedef void (*DataCallBack)(char* topic, uint8_t* payload, uint16_t length);
    ConnectionListener connectionListener = NULL;
    WiFiClientSecure wifiClient;
    PubSubClient client;
    char 
             id[50], 
            mac[20], 
       password[40], 
      topicSend[86], 
             ca[2048], 
      willTopic[60];

    bool startMQTT, preConnect;
    int preState;
    void _Connect();
    void printCause(int state);
  public:
    MQTT();
    void init();
    void setCallBack(DataCallBack callBack);
    void Connect(const char* server, uint16_t port, const char* mac, const char* id, const char* password);
    void send(const char* command, const char* data);
    void setOnMQTTConnection(ConnectionListener listener);
    void loop();
};

#endif