#include "MainTask.h"
#include "ConnectionTask.h"
#include "FS.h"
#include "SPIFFS.h"

QueueHandle_t receiveQueue, sendQueue;
uint32_t resetTimer;
uint8_t  resetCounter;

void ResetReason(char* reason) {
  esp_reset_reason_t r = esp_reset_reason();
  switch (r) {
    case (ESP_RST_UNKNOWN):   strcpy_P(reason, (const char*)F("Reset reason can not be determined."));                     break;
    case (ESP_RST_POWERON):   strcpy_P(reason, (const char*)F("Reset due to power-on event."));                            break;
    case (ESP_RST_EXT):       strcpy_P(reason, (const char*)F("Reset by external pin (not applicable for ESP32)"));        break;
    case (ESP_RST_SW):        strcpy_P(reason, (const char*)F("Software reset via esp_restart."));                         break;
    case (ESP_RST_PANIC):     strcpy_P(reason, (const char*)F("Software reset due to exception/panic."));                  break;
    case (ESP_RST_INT_WDT):   strcpy_P(reason, (const char*)F("Reset (software or hardware) due to interrupt watchdog.")); break;
    case (ESP_RST_TASK_WDT):  strcpy_P(reason, (const char*)F("Reset due to task watchdog."));                             break;
    case (ESP_RST_WDT):       strcpy_P(reason, (const char*)F("Reset due to other watchdogs."));                           break;
    case (ESP_RST_DEEPSLEEP): strcpy_P(reason, (const char*)F("Reset after exiting deep sleep mode."));                    break;
    case (ESP_RST_BROWNOUT):  strcpy_P(reason, (const char*)F("Brownout reset (software or hardware)"));                   break;
    case (ESP_RST_SDIO):      strcpy_P(reason, (const char*)F("Reset over SDIO."));                                        break;
  }
}

void coreTask(void *parameter) {
  MainTask app(sendQueue, receiveQueue);
  app.setResetReason(ResetReason);
  app.init();
  while (true) {
    app.loop();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

void connectionTask(void *parameter) {
  ConnectionTask app(receiveQueue, sendQueue);
  app.setResetReason(ResetReason);
  app.init();
  while (true) {
    app.loop();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

void setup(void) {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
    Serial.println("Formmating file system...");
    SPIFFS.format();
    Serial.println("Format done.");
  }
  
  resetCounter = 0;
  File file = SPIFFS.open("/reset.dat", "r");
  if (file) {
    resetCounter = file.read();
    if (resetCounter < 255)
      resetCounter++;
    resetTimer = millis();
    file.close();
    if (resetCounter >= 5) {
      Serial.println(F("\n\nReset is in progress..."));
      SPIFFS.format();
      resetCounter = 0;
      SPIFFS.open("/reset.dat", "w").write(resetCounter);
      ESP.restart();
    }
  }
  SPIFFS.open("/reset.dat", "w").write(resetCounter);
  SPIFFS.open("/ResetCause.dat", "a").write(esp_reset_reason());
  
  receiveQueue = xQueueCreate(4096, 1);
  sendQueue    = xQueueCreate(4096, 1);
  xTaskCreate(coreTask, "coreTask", 30 * 1024, NULL, 1, NULL);
  xTaskCreate(connectionTask, "connectionTask", 30 * 1024, NULL, 1, NULL);
}

void loop(void) {
  if (resetTimer > 0)
    if (millis() - resetTimer >= 5000) {
      resetTimer = 0;
      resetCounter = 0;
      File file = SPIFFS.open("/reset.dat", "w");
      file.write(resetCounter);
    }
}
