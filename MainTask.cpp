#include "MainTask.h"

MainTask *toThis;
static void shortcut_DataTransformListener(char* command, char* payload) { toThis->DataTransformListener(command, payload); }

MainTask::MainTask(QueueHandle_t sender, QueueHandle_t receiver) {
  dataTransform.setName("Main");
  dataTransform.setSendQueue(send);
  dataTransform.setReceiveQueue(receive);
  dataTransform.setDataListener(shortcut_DataTransformListener);
  oneWire.begin(23);
  ds18b20.setOneWire(&oneWire);
  ds18b20.begin();
  moisturePin = 36;
}

/**
* @brief Function for saving config data
*/
void MainTask::saveConfig() {
  JsonDocument doc;
  doc["interval"] = readingInterval;
  char buffer[100];
  serializeJson(doc, buffer, 100);
  file.fWrirte("/config.dat", buffer);
}

/**
* @brief Function for loading pre-saved config data
*/
void MainTask::readConfig() {
  JsonDocument doc;
  char buffer[1024];
  bool successful = false;
  if (file.fRead("/config.dat", buffer, 1024)) {                //Check the file is exists or data can be read
    DeserializationError error = deserializeJson(doc, buffer);  //Check the read data is valid JSON format?
    if (!error) {
      readingInterval = doc.containsKey("interval") ? doc["interval"].as<uint16_t>() : 60;
      successful = true;
    }
  }

  if (!successful) //If reading the required config was not successful write over
    writeConfig();
}

/**
* @brief Received requests from ConnectionTask
* @param command Part of Command
* @param payload Part of Payload
*/
void MainTask::DataTransformListener(char *command, char *payload) {
  char buffer[1024];
  JsonDocument;

  //If mqtt connection is connected succesfully then send the read information
  if (strcmp(command, "MQTT.Connected") == 0) {
    doc["Moisture"]    = moisture;
    doc["Temperature"] = temperature;
    serializeJson(doc, buffer, 100);
    dataTransform.sendData("RequiredInformation", buffer);
  }

  //If a message is received from mqtt server then read the interval
  else if (strcmp(command, "MQTT.Received") == 0) {
    deserializeJson(doc, buffer);
    readingInterval = doc["Interval"].as<uint16_t>();
    saveConfig();
  }

  //If mqtt message is sent succesfully then go to sleep
  else if (strcmp(command, "MQTT.Sent") == 0) {
    esp_sleep_enable_timer_wakeup(interval * 60000000 /*Convert given time to microseconds*/);
    esp_deep_sleep_start();
  }
}


void MainTask::Initialize() {

  //Get the wakeup reason
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  //Read the required information from sensors
  ds18b20.requestTemperatures();
  temperature = ds18b20.getTempCByIndex(0);

  /*
    DryValue and WetValue are achieved when the sensor is in completly wet and dry.
    Below numbers are approximately, and with the map function calculate the percent
  */
  moisture    = map(analogRead(moisturePin), 3500 /*Dry Value*/, 1500 /*Wet Value*/, 0, 100);
  timeStamp   = millis(); 

  /*
    If the module is waked up by push button:
      1- show on the display
      2- dim the display after 10 seconds
  */
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {    
    display.showInformation(moisture, temperature);
    display.dim(false); //Show the information on the display and turn it on

    //Dim the display after 10 seconds
    ticker1.once(10, []{ toThis->display.dim(true); });
  }

  readConfig();

  /*
    Reading interval and sending current data should be done in less than 30 seconds
    Becase the module go to sleep in 30 seconds after startup
  */
  ticker2.once(30, []{ 
    esp_sleep_enable_timer_wakeup(interval * 60000000 /*Convert given time to microseconds*/);
    esp_deep_sleep_start();
  });
}

void MainTask::Loop() {

}