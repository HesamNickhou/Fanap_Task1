#include "MainTask.h"
#include "ConnectionTask.h"

QueueHandle_t receiveQueue, sendQueue;

void mainTask(void *parameter) {
  MainTask app(sendQueue, receiveQueue);
  app.Initialize();
  while (true) {
    app.Loop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void connectionTask(void *parameter) {
  ConnectionTask app(receiveQueue, sendQueue);
  while (true) {
    app.loop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void setup(void) {
  Serial.begin(115200);

  //Prepare the File Flash system, if it is not ready
  if (!SPIFFS.begin(true)) {
    Serial.println("Formmating file system...");
    SPIFFS.format();
    Serial.println("Format done.");
  }
  
  //Create tasks and their buffers for message transferring
  receiveQueue = xQueueCreate(4096, 1);
  sendQueue    = xQueueCreate(4096, 1);
  xTaskCreate(mainTask, "coreTask", 30 * 1024, NULL, 1, NULL);
  xTaskCreate(connectionTask, "connectionTask", 30 * 1024, NULL, 1, NULL);
}

void loop(void) {
  //Nothing
}
