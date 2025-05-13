#include "ConnectionTask.h"

WebServer server(80);

//Define the static functions to be used in lambda
ConnectionTask *toThis1;
static void shortcut_MQTTReceived(char* topic, uint8_t* payload, uint16_t length)     { toThis1->MQTTReceived(topic, payload, length); }
static void shortcut_wifiConnection(bool isConnected, const char* reason)             { toThis1->wifiConnection(isConnected, reason); }
static void shortcut_MQTTConnected(bool isConnected)                                  { toThis1->MQTTConnected(isConnected); }
static void shortcut_wifiDataReceived(char* command, uint8_t* payload, uint16_t size) { toThis1->WifiDataReceived(command, payload, size); }
static void shortcut_DataTransformListener(char* command, char* payload)              { toThis1->DataTransformListener(command, payload); }

/**
* @brief Starter function to initialize values
* @param send The sending buffer for freeRTOS
* @param receive The receving buffer for freeRTOS
*/
ConnectionTask::ConnectionTask(QueueHandle_t send, QueueHandle_t receive) {
  toThis1 = this;
  
  //Initialize the Datatransformer for Connection Task
  dataTransform.setName("Connect");
  dataTransform.setSendQueue(send);
  dataTransform.setReceiveQueue(receive);
  dataTransform.setDataListener(shortcut_DataTransformListener);

  //Send the MAC address for Main Task to start the routines
  wifi.getMAC(MAC);
  dataTransform.sendData("Internal.WIFI.MAC", MAC);
  
  //Initialize MQTT callbacks
  mqtt.setOnMQTTConnection(shortcut_MQTTConnected);
  mqtt.setCallBack(shortcut_MQTTReceived);

  currentTime = 0;
}

/**
* @brief Sets the reset reason callback. The reason will be send to the server for future analysis
* @param interface (char* reason)
*/
void ConnectionTask::setResetReason(ResetReason interface) { resetReason = interface; }

void ConnectionTask::MainTimer() {
  doMainTimer = false;

  currentTime++;
  if (currentTime == 10080)
    currentTime = 0;
  uint8_t day = (currentTime / 1440) + 1;
  uint8_t hour = (currentTime % 1440) / 60;
  uint8_t minute = (currentTime % 1440) % 60;  
  
  JsonDocument doc;
  doc["Time"]      = hour * 60 + minute;
  doc["DayOfWeek"] = day;

  
  if (temp == -127)
    temp = 255;
  doc["Temperature"] = temp;
  char buffer[100];
  serializeJson(doc, buffer);
  dataTransform.sendData("Internal.MainTimer", buffer);

  if ((hour == 0) && (minute == 0))
    sendCommand("GetServerTime", "");
}

void ConnectionTask::wifiConnection(bool isConnected, const char* reason){
  if (Mode == 2) {
    char ip[16];
    sprintf(ip, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    
    JsonDocument doc;
    doc["Status"] = isConnected;
    doc["IP"]     = ip;
    doc["Reason"] = reason;
    char buffer[100];
    serializeJson(doc, buffer);
    dataTransform.sendData("Internal.WIFI.Connection", buffer);
  }
}

void ConnectionTask::wifiRSSI(uint8_t quality) {}

void ConnectionTask::MQTTConnected(bool isConnected){
  JsonDocument doc;
  doc["Connected"] = isConnected;
  char buffer[50];
  serializeJson(doc, buffer);
  dataTransform.sendData("Internal.MQTT.Connection", buffer);
  mqttConnected = isConnected;
  if (isConnected && firstTime) {
    firstTime = false;
    dataTransform.sendData("WhoIs", "");
    mqtt.send("GetServerTime", "");
    
    char reason[100];
    resetReason(reason);
    mqtt.send("ResetReason", reason);
  }
  Serial.printf("\n\tMDNS is %s\n", wifi.mdns("NP-H31") ? "OK" : "Failed");
}

void ConnectionTask::MQTTReceived(char* topic, uint8_t* payload, uint16_t length) {
  uint16_t index;
  for (uint8_t i=0; topic[i] != NULL; i++)
    if (topic[i] == '/')
      index = i;

  char command[128];
  strcpy(command, topic + index + 1);

  if (strcmp(command, "OTA.Start") == 0) {
    sendCommand("OTA.Start", "");
    payload[length] = NULL;
    wifi.send("Update.GetFile", (char*) payload, OTA_Server, OTA_Port);
  }
  
  else if (strcmp(command, "OTA.Stop") == 0) {
    wifi.stop();
    ota.abort();
    sendCommand("OTA.Stop", "");
  }
  
  else if (strcmp(command, "OTA.Restart") == 0) {
    sendCommand("OTA.Restart", "");
    mainTicker.attach(3, []{ ESP.restart(); });
  }
  
  else if (strcmp(command, "OTA.Rollback") == 0) {
    Update.rollBack();
    sendCommand(command, "");
    mainTicker.attach(1, []{ ESP.restart(); });
  }

  else
    dataTransform.sendData(command, (char *) payload, (uint16_t) length);
}

void ConnectionTask::WifiDataReceived(char* command, uint8_t* payload, uint16_t size) {
  if (strncmp(command, "Update.", 7) == 0) 
    ota.execute(command, payload, size);

  else if (strcmp(command, "OTA.Restart") == 0)
    ESP.restart();
  
  else if (strcmp(command, "OTA.Rollback") == 0) {
    Update.rollBack();
    ESP.restart();
  }
    
  else if (strcmp(command, "Certificate") == 0) {
    uint16_t i = 0;
    File f = SPIFFS.open("/certificate.ca", "w");
    while (i < size)
      f.write(payload[i++]);
    f.close();
    wifi.send("Certificate", "Received");
  }
  
  else
    dataTransform.sendData(command, (char*) payload, size);
}

void ConnectionTask::DataTransformListener(char* command, char* payload) {
  if (strcmp(command, "Internal.Setting") == 0) {
    JsonDocument doc;
    deserializeJson(doc, payload);
    strcpy(APSSID, doc["APSSID"].as<const char*>());
    strcpy(APPWD, doc["APPWD"].as<const char*>());
    Mode = doc["Mode"].as<uint8_t>();
    strcpy(Gmail, doc["Gmail"].as<const char*>());
    strcpy(Password, doc["Password"].as<const char*>());
    strcpy(MQTT_Server, doc["MQTTServer"].as<const char*>());
    MQTT_Port = doc["MQTTPort"].as<uint16_t>();
    strcpy(OTA_Server, doc["OTAServer"].as<const char*>());
    OTA_Port = doc["OTAPort"].as<uint16_t>();
    wifi.setWifiReboot(doc["WifiReboot"].as<uint8_t>());
    wifi.init(APSSID, APPWD, Mode);
    if (Mode == 1) {
      server.begin();
      Serial.println("\n\tHTTP server started");
      Serial.println(WiFi.softAPIP());

      Serial.printf("\n\tMDNS is %s\n", wifi.mdns("NP-H31") ? "OK" : "Failed");
      server.on("/", HTTP_GET, [this]() {
        if (server.args() > 0) {
          Mode = 2;
          if (server.hasArg("APSSID"))     server.arg("APSSID").toCharArray(APSSID, 50);
          if (server.hasArg("APPWD"))      server.arg("APPWD").toCharArray(APPWD, 32);
          if (server.hasArg("Gmail"))      server.arg("Gmail").toCharArray(Gmail, 50);
          if (server.hasArg("Password"))   server.arg("Password").toCharArray(Password, 37);
          if (server.hasArg("MQTTServer")) server.arg("MQTTServer").toCharArray(MQTT_Server, 50);
          if (server.hasArg("OTAServer"))  server.arg("OTAServer").toCharArray(OTA_Server, 50);
          if (server.hasArg("MQTTPort"))   MQTT_Port = (uint16_t) server.arg("MQTTPort").toInt();
          if (server.hasArg("OTAPort"))    OTA_Port =  (uint16_t) server.arg("OTAPort").toInt();
          if (server.hasArg("Mode"))       Mode = (uint8_t) server.arg("Mode").toInt();
          if (server.hasArg("CA")) {
            File f = SPIFFS.open("/certificate.ca", "w");
            f.print(server.arg("CA"));
            f.close();
          }

          Serial.println("Writting values to file");
          JsonDocument doc;
          doc["APSSID"]     = APSSID;
          doc["APPWD"]      = APPWD;
          doc["Mode"]       = Mode;
          doc["Gmail"]      = Gmail;
          doc["Password"]   = Password;
          doc["MQTTServer"] = MQTT_Server;
          doc["MQTTPort"]   = MQTT_Port;
          doc["OTAServer"]  = OTA_Server;
          doc["OTAPort"]    = OTA_Port;
          File f = SPIFFS.open("/settings.dat", "w");
          serializeJson(doc, f);
          f.close();

          Serial.println("Settings is received");
          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", "OK");
          ESP.restart();
        }

        else {
          server.sendHeader("Connection", "close");
          server.send(200, "text/html", serverIndex);
        }
      });

      server.on("/", HTTP_POST, [this]() {
          Serial.println(server.uri());
          server.sendHeader("Connection", "close");
          server.send(200, "text/html", Update.hasError() ? server_failed : server_success);
          if (!Update.hasError())
            mainTicker.attach(2, []{ ESP.restart(); });
        }, 

        [this]() {
          HTTPUpload& upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            sunrise = 0;
            if (strstr(upload.filename.c_str(), ".bin") == NULL) {
              Serial.println("Bad file format");
              server.sendHeader("Connection", "close");
              server.send(200, "text/html", server_badfile);
              server_error = true;
            }

            else {
              server_error = false;
              Serial.printf("Update: %s\n", upload.filename.c_str());
              uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
              if (!Update.begin(maxSketchSpace)) { //start with max available size
                Update.printError(Serial);
              }
            }
          }
          
          else if ((upload.status == UPLOAD_FILE_WRITE) && (!server_error)) {
            sunrise++;
            if (sunrise >= 100) {
              Serial.println(".");
              sunrise = 0;
            }
            else
              Serial.print(".");
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              Update.printError(Serial);
              server_error = true;
            }
          }
          
          else if ((upload.status == UPLOAD_FILE_END) && (!server_error)) {
            if (Update.end(true))
              Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            else
              Update.printError(Serial);
          }
      });
      server.begin();
    }
    else
      mqtt.init();
  } 
  
  else if (strcmp(command, "Internal.MQTT.Connect") == 0) {
    mqtt.Connect(MQTT_Server, MQTT_Port, MAC, Gmail, Password);
  } 
  
  else if (strcmp(command, "Internal.Get.Temperature") == 0) {
    JsonDocument doc;
    temperature.requestTemperatures();
    int temp = temperature.getTempCByIndex(0);
    if (temp == -127)
      temp = 255;
    doc["Temperature"] = temp;
    char buffer[50];
    serializeJson(doc, buffer, 50);
    dataTransform.sendData(command, buffer);
  } 
  
  else if (strcmp(command, "Internal.Time.Set") == 0) {
    JsonDocument doc;
    deserializeJson(doc, payload);
    uint8_t minute = doc["Minute"].as<uint8_t>();
    uint8_t hour   = doc["Hour"].as<uint8_t>();
    uint8_t day    = doc["DayOfWeek"].as<uint8_t>();
    currentTime = ((day - 1) * 1440) + ((hour * 60) + minute);
  } 
  
  else if (strcmp(command, "Temperature") == 0) {
    JsonDocument doc;
    deserializeJson(doc, payload);
    uint8_t bias = doc["Bias"].as<uint8_t>();
    doc.clear();
    temperature.requestTemperatures();
    int temp = temperature.getTempCByIndex(0);
    if (temp == -127)
      temp = 255;
    else
      temp -= bias;
    doc["Temperature"] = temp;
    char buffer[50];
    serializeJson(doc, buffer);
    sendCommand(command, buffer);
  } 
  
  else if (strcmp(command, "TempTime") == 0) {
    JsonDocument doc;
    deserializeJson(doc, payload);
    uint8_t bias = doc["Bias"].as<uint8_t>();
    doc.clear();
    temperature.requestTemperatures();
    int temp = temperature.getTempCByIndex(0);
    if (temp == -127)
      temp = 255;
    else
      temp -= bias;  
    doc["Temperature"] = temp;

    doc["Time"] = currentTime % 1440;
    doc["DayOfWeek"] = (currentTime / 1440) + 1;
    
    char buffer[100];
    serializeJson(doc, buffer);
    sendCommand(command, buffer);
  }

  else if (strcmp(command, "Internal.WIFI.Reboot") == 0) {
    JsonDocument doc;
    deserializeJson(doc, payload);
    wifi.setWifiReboot(doc["Wifi"].as<uint8_t>());
  }

  else
    sendCommand(command, payload);
}

void ConnectionTask::sendCommand(const char* command, const char* payload) {
  if (Mode < 3 || (strcmp(command, "SetSettings") == 0)) {
    if (mqttConnected)
      mqtt.send(command, payload);
      
    if (!otaStarted) {
      wifi.send(command, payload);
      Serial.printf("<< %s = %s\n", command, payload);
    }   

    if (strcmp(command, "SetSettings") == 0)
      ESP.restart();
  }
}

void ConnectionTask::setWDT(bool value) {
  if (value)
    esp_task_wdt_add(NULL);
  else
    esp_task_wdt_delete(NULL);
}

void ConnectionTask::init() {
  mqtt.setOnMQTTConnection(shortcut_MQTTConnected);
  wifi.setOnWifiConnection(shortcut_wifiConnection);
  wifi.setOnDataReceived(shortcut_wifiDataReceived);
  wifi.setOnRSSIListener(shortcut_rssiListener);
  wifi.setOnRestartListener([] { toThis1->dataTransform.sendData("Internal.WIFI.Restart", ""); });

  if (Mode == 1) {
    Serial.print("\n\t");
    Serial.println(WiFi.softAPIP());
  }  
  mainTicker.attach(60, []{ toThis1->doMainTimer = true; });
}

void ConnectionTask::loop() {
  wifi.loop();
  mqtt.loop();
  dataTransform.loop();
  if (Mode == 1) server.handleClient();
  
  if (doMainTimer)  
    MainTimer();
}
