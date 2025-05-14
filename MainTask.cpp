#include "MainTask.h"

//Static functions which will be used in callbacks or lambda
MainTask *toThis;
static void shortcut_DataTransformListener(char* command, char* payload) { toThis->DataTransformListener(command, payload); }
static void shortcut_ButtonListener(uint8_t state) { toThis->ButtonListener(state); }

/**
Initializer function for Main Task
*/
MainTask::MainTask(QueueHandle_t sender, QueueHandle_t receiver) {
  dataTransform.setName("Main");
  dataTransform.setSendQueue(sender);
  dataTransform.setReceiveQueue(receiver);
  dataTransform.setDataListener(shortcut_DataTransformListener);
  oneWire.begin(23);
  ds18b20.setOneWire(&oneWire);
  ds18b20.begin();
  moisturePin = 36;
  button.setParameters(33, HIGH);
  button.setOnTouchListener(shortcut_ButtonListener);
}

/**
When push button is pressed then the callback sends 1 for value of state
When the push button is released then value of 2 will be sent
When at least 3 seconds pressed, then the value of 3 is the result!
*/
void MainTask::ButtonListener(uint8_t state) {
  //If push button is long pressed at least for 3 seconds then reset the device
  if (state == 3) {
    setting.Mode == 1;
    strcpy(setting.SSID, "");
    strcpy(setting.PWD, "");
    strcpy(setting.MQTT_Id, "");
    strcpy(setting.MQTT_Password, "");
    strcpy(setting.MQTT_Server, "");
    setting.MQTT_Port = 8083;
    setting.readingInterval = 1440; //1440 minutes mean 24 hours
    saveConfig();
    ESP.restart();
  }
}

/**
* Function for saving config data
*/
void MainTask::saveConfig() {
  JsonDocument doc;
  doc["SSID"]     = setting.SSID;
  doc["PWD"]      = setting.PWD;
  doc["Id"]       = setting.MQTT_Id;
  doc["Password"] = setting.MQTT_Password;
  doc["Server"]   = setting.MQTT_Server;
  doc["Port"]     = setting.MQTT_Port;
  doc["Mode"]     = setting.Mode;
  doc["interval"] = setting.readingInterval;

  char buffer[1024];
  serializeJson(doc, buffer, 1024);
  file.fWrite("/config.dat", buffer);
}

/**
* Function for loading pre-saved config data
*/
void MainTask::readConfig() {
  JsonDocument doc;
  char buffer[1024];
  bool successful = false;
  if (file.fRead("/config.dat", buffer, 1024)) {                //Check the file is exists or data can be read
    DeserializationError error = deserializeJson(doc, buffer);  //Check the read data is valid JSON format?
    if (!error) {
      strcpy(setting.SSID           , doc.containsKey("SSID") ?     doc["SSID"].as<const char*>() : "");
      strcpy(setting.PWD            , doc.containsKey("PWD") ?      doc["PWD"].as<const char*>() : "");
      strcpy(setting.MQTT_Id        , doc.containsKey("Id") ?       doc["Id"].as<const char*>() : "");
      strcpy(setting.MQTT_Password  , doc.containsKey("Password") ? doc["Password"].as<const char*>() : "");
      strcpy(setting.MQTT_Server    , doc.containsKey("Server") ?   doc["Server"].as<const char*>() : "Fanap.com");
      setting.MQTT_Port             = doc.containsKey("Port") ?     doc["Port"].as<uint16_t>() : 8083;
      setting.Mode                  = doc.containsKey("Mode") ?     doc["Mode"].as<uint8_t>() : 1;
      setting.readingInterval       = doc.containsKey("interval") ? doc["interval"].as<uint16_t>() : 60;
      successful = true;
    }
  }

  if (!successful) //If reading the required config was not successful write over
    saveConfig();
}

/**
* Received requests from ConnectionTask
* command Part of Command
* payload Part of Payload
*/
void MainTask::DataTransformListener(char *command, char *payload) {
  char buffer[1024];
  JsonDocument doc;

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
    setting.readingInterval = doc["Interval"].as<uint16_t>();
    saveConfig();
  }

  //If mqtt message is sent succesfully then go to sleep
  else if (strcmp(command, "MQTT.Sent") == 0) {
    esp_sleep_enable_timer_wakeup(setting.readingInterval * 60000000 /*Convert given time to microseconds*/);
    esp_deep_sleep_start();
  }

  else if (strcmp(command, "WiFi.Settings") == 0) {
    deserializeJson(doc, payload);
    strcpy(setting.SSID,          doc["SSID"].as<const char*>());
    strcpy(setting.PWD,           doc["PWD"].as<const char*>());
    strcpy(setting.MQTT_Id,       doc["Id"].as<const char*>());
    strcpy(setting.MQTT_Password, doc["Password"].as<const char*>());
    strcpy(setting.MQTT_Server,   doc["Server"].as<const char*>());
    setting.MQTT_Port =           doc["Port"].as<uint16_t>();
    saveConfig();
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
  //Read connection configuration and send it via freeRTOS Queue to the ConnectionTask
  JsonDocument doc;
  doc["SSID"]     = setting.SSID;
  doc["PWD"]      = setting.PWD;
  doc["Id"]       = setting.MQTT_Id;
  doc["Password"] = setting.MQTT_Password;
  doc["Server"]   = setting.MQTT_Server;
  doc["Port"]     = setting.MQTT_Port;
  doc["Mode"]     = setting.Mode;
  char buffer[512];
  serializeJson(doc, buffer, 512);
  dataTransform.sendData("Internal.Setting", buffer);

  
  //If the module is in initialize mode and isn't connected to the internet, is not neccessary to go to sleep mode
  if (setting.Mode > 1)
  /*
    Reading interval and sending current data should be done in less than 30 seconds
    Becase the module go to sleep in 30 seconds after startup
  */
    ticker2.once(30, [toThis]{ 
      esp_sleep_enable_timer_wakeup(toThis->setting.readingInterval * 60000000 /*Convert given time to microseconds*/);
      esp_deep_sleep_start();
    });
}

void MainTask::Loop() {
  dataTransform.loop();
  button.loop();
}