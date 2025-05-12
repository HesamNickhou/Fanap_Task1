#ifndef DATATRANSFORM_h
#define DATATRANSFORM_h
#include <Arduino.h>

class DataTransform {
  private:
    typedef void (* DataReceived) (char* command, char* payload);
    char command[96], payload[4000], name[10];
    uint8_t step;
    bool debug;
    uint16_t counter;
    char element;
    QueueHandle_t sender, receiver;
    DataReceived listener;
  public:
    DataTransform();
    void setName(const char* taskName);
    void setSendQueue(QueueHandle_t queue);
    void setReceiveQueue(QueueHandle_t queue);
    void sendData(const char* cmd, const char* pld);
    void sendData(const char* cmd, const char* pld, uint16_t size);
    void setDataListener(DataReceived callBack);
    void loop();
};
#endif
