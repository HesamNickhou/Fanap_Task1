#include "ConnectionTask.h"

WebServer server(80);

//Define the static functions to be used in lambda
ConnectionTask *toThis1;
static void shortcut_MQTTReceived(char* topic, uint8_t* payload, uint16_t length)     { toThis1->MQTTReceived(topic, payload, length); }
static void shortcut_wifiConnection(bool isConnected)                                 { toThis1->wifiConnection(isConnected); }
static void shortcut_MQTTConnected(bool isConnected)                                  { toThis1->MQTTConnected(isConnected); }
static void shortcut_wifiDataReceived(char* command, uint8_t* payload, uint16_t size) { toThis1->WifiDataReceived(command, payload, size); }
static void shortcut_DataTransformListener(char* command, char* payload)              { toThis1->DataTransformListener(command, payload); }

/**
* Starter function to buuffers(QueueHandle_t)
*/
ConnectionTask::ConnectionTask(QueueHandle_t send, QueueHandle_t receive) {
  toThis1 = this;
  
  //Initialize the Datatransformer for Connection Task
  dataTransform.setName("Connect");
  dataTransform.setSendQueue(send);
  dataTransform.setReceiveQueue(receive);
  dataTransform.setDataListener(shortcut_DataTransformListener);

  //Sets MQTT and WiFi connections callback
  mqtt.setOnMQTTConnection(shortcut_MQTTConnected);
  wifi.setOnWifiConnection(shortcut_wifiConnection);

  //Send the MAC address for Main Task to start the routines
  wifi.getMAC(MAC);
  dataTransform.sendData("Internal.WIFI.MAC", MAC);
  
  //Initialize MQTT callbacks
  mqtt.setOnMQTTConnection(shortcut_MQTTConnected);
  mqtt.setCallBack(shortcut_MQTTReceived);
}

/**
  Wifi connection callback.
  If Wifi is connected to the remote modem then connect to the MQTT server with right parameters
*/
void ConnectionTask::wifiConnection(bool isConnected){
  if (isConnected)
    mqtt.Connect(MQTT_Server, MQTT_Port, MAC, Id, Password);
}

/**
  MQTT connection callback
  If MQTT is connected to the server then notify Main Task to send its required information
*/
void ConnectionTask::MQTTConnected(bool isConnected){
  if (isConnected)
    dataTransform.sendData("MQTT.Connected", "");
  mqttConnected = isConnected;
}

/**
  MQTT data received callback.
  When a message is arrived, firstly extract the command and then send it via buffer for Main Task
*/
void ConnectionTask::MQTTReceived(char* topic, uint8_t* payload, uint16_t length) {

  /*
    MQTT Topic: Fanap/Id/Task/MAC/Command
    The below code is extracting the Command part of topic
  */
  uint16_t index;
  for (uint8_t i=0; topic[i] != NULL; i++)
    if (topic[i] == '/')
      index = i;

  char command[128];
  strcpy(command, topic + index + 1);
  dataTransform.sendData(command, (char *) payload, (uint16_t) length); //This goes for MainTask buffer
}

/**
  WiFi data received callback
  The module is not connected to any modem or server in factory mode.
  After startup an application should give the credentials (for both of wifi modem and mqtt user/pass) via TCP
  A certificate file is required to establish a secured connection to the MQTT server
*/
void ConnectionTask::WifiDataReceived(char* command, uint8_t* payload, uint16_t size) {

  //If application sends message in form of Certificate[DataArray], we should save in the file system
  if (strcmp(command, "Certificate") == 0) {
    uint16_t i = 0;
    File f = SPIFFS.open("/certificate.ca", "w");
    while (i < size)
      f.write(payload[i++]);
    f.close();
    wifi.send("Certificate", "Received");
  }
  
  //Wifi settings which should be go for Main Task
  else if (strcmp(command, "WiFi.Setting"))
    dataTransform.sendData(command, (char*) payload);
}

/**
  In tasks buffer data received callback
  Every message which goes to the buffer, will be shown here
*/
void ConnectionTask::DataTransformListener(char* command, char* payload) {

  //Wifi setting which main task sends here to establish WiFi connection
  if (strcmp(command, "Internal.Setting") == 0) {
    JsonDocument doc;
    deserializeJson(doc, payload);
    strcpy(SSID,        doc["SSID"].as<const char*>());
    strcpy(PWD,         doc["PWD"].as<const char*>());
    Mode =              doc["Mode"].as<uint8_t>();
    strcpy(Id,          doc["Id"].as<const char*>());
    strcpy(Password,    doc["Password"].as<const char*>());
    strcpy(MQTT_Server, doc["Server"].as<const char*>());
    MQTT_Port =         doc["Port"].as<uint16_t>();
    wifi.init(SSID, PWD, Mode);
    
    if (Mode == 1) { //Mode=1 means module is not connected to anywhere and it should be a webserver to get data from user
      server.begin();
      Serial.println("\n\tHTTP server started");
      Serial.println(WiFi.softAPIP());

      server.on("/", HTTP_GET, [this]() {
        if (server.args() > 0) {
          Mode = 2;
          if (server.hasArg("SSID"))     server.arg("SSID").toCharArray(SSID, 50);
          if (server.hasArg("PWD"))      server.arg("PWD").toCharArray(PWD, 32);
          if (server.hasArg("Id"))       server.arg("Id").toCharArray(Id, 50);
          if (server.hasArg("Password")) server.arg("Password").toCharArray(Password, 37);
          if (server.hasArg("Server"))   server.arg("Server").toCharArray(MQTT_Server, 50);
          if (server.hasArg("Port"))     MQTT_Port = (uint16_t) server.arg("Port").toInt();
          if (server.hasArg("Mode"))     Mode = (uint8_t) server.arg("Mode").toInt();
          if (server.hasArg("CA")) {
            File f = SPIFFS.open("/certificate.ca", "w");
            f.print(server.arg("CA"));
            f.close();
          }

          Serial.println("Writting values to file");
          JsonDocument doc;
          doc["SSID"]       = SSID;
          doc["PWD"]        = PWD;
          doc["Mode"]       = Mode;
          doc["Id"]         = Id;
          doc["Password"]   = Password;
          doc["MQTTServer"] = MQTT_Server;
          doc["MQTTPort"]   = MQTT_Port;
          File f = SPIFFS.open("/config.dat", "w");
          serializeJson(doc, f);
          f.close();

          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", "OK");
          ESP.restart();
        }

        else {
          server.sendHeader("Connection", "close");
          server.send(200, "text/html", "HTML Code!");
        }
      });

      server.begin();
    }

    //Else if module should be connect to remo modem, then initialize the mqtt object
    else
      mqtt.init();
  } 
  
  //Any other undefined message which come from main task, then send it via the MQTT object
  else
    sendCommand(command, payload);
}

//Send messages via MQTT
void ConnectionTask::sendCommand(const char* command, const char* payload) {
  if (mqttConnected)
    mqtt.send(command, payload);
}

//Do object infinite jobs
void ConnectionTask::loop() {
  wifi.loop();
  mqtt.loop();
  dataTransform.loop();
  if (Mode == 1) server.handleClient();
}
