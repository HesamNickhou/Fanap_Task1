#include "MQTT.h"

/**
* Defining and pre-intialize the object
*/
MQTT::MQTT() {
  connectionListener = NULL;  
  startMQTT          = false;
  preConnect         = false;
  preState           = -1000;
}

/**
* Initialize the instance, load the certificate data for tls
*/
void MQTT::init() {
  //CA of tls is stored in certificate.ca file previously. It should be load before start the connection
  File f = SPIFFS.open("/certificate.ca", "r");
  if (f) {
    uint16_t i = 0;
    while (f.available())
      ca[i++] = f.read();
    ca[i] = NULL;
    f.close();
    wifiClient.setCACert(ca);
  }
  
  else
    Serial.println("Certificate isn't exists");
  
  client.setBufferSize(4096);
  client.setKeepAlive(60);
  client.setClient(wifiClient);
}

/**
* Print the connection failur cause in Serial output for debugging purpose
* state Is received by the PubSubClient's feedback
*/
void MQTT::printCause(int state) {
  if (state != 0)
    Serial.print(F("MQTT is "));
  switch (state) {
    case -4: Serial.println(F("CONNECTION_TIMEOUT - the server didn't respond within the keepalive time")); break;
    case -3: Serial.println(F("CONNECTION_LOST - the network connection was broken")); break;
    case -2: Serial.println(F("CONNECT_FAILED - the network connection failed")); break;
    case -1: Serial.println(F("DISCONNECTED - the client is disconnected cleanly")); break;
    case 0: /*Serial.println(F("CONNECTED - the client is connected"));*/ break;
    case 1: Serial.println(F("CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT")); break;
    case 2: Serial.println(F("CONNECT_BAD_CLIENT_ID - the server rejected the client identifier")); break;
    case 3: Serial.println(F("CONNECT_UNAVAILABLE - the server was unable to accept the connection")); break;
    case 4: Serial.println(F("CONNECT_BAD_CREDENTIALS - the username/password were rejected")); break;
    case 5: Serial.println(F("CONNECT_UNAUTHORIZED - the client was not authorized to connect")); break;
    default: Serial.println(F("Unknown error!"));
  }
}

/**
* Sets the MQTT data received callback
* callBack (char* topic, uint8_t* payload, uint16_t length)
*/
void MQTT::setCallBack(DataCallBack callBack) {
  client.setCallback(callBack);
}

/**
* Internal connect function to establish MQTT connection
*/
void MQTT::_Connect() {
  startMQTT = true;
  if (client.connect(mac, id, password, willTopic, 0, false, mac, false)) {
    /*
    Format of transfering topics is: 
    From Device to Server:      Fanap/<UserID>/<Project-Module name>/<MAC>/App
    From Application to Device: Fanap/<UserID>/<Project-Module name>/<MAC>/Gateway

    Device subscribes to: 
      Fanap/<User ID>/WhoIs       //This is for finding devices under an user modules
      Fanap/Server/+              //This is for broadcasting a message to all devices
    */

    sprintf(topicSend, "Fanap/%s/Task1/%s/App", id, mac);

    client.subscribe("Fanap/Time");
    char topicReceive[82];
    sprintf(topicReceive, "Fanap/%s/WhoIs", id);
    client.subscribe(topicReceive);

    sprintf(topicReceive, "Fanap/%s/Task1/%s/Gateway/+", id, mac);
    client.subscribe(topicReceive);
    client.subscribe("Fanap/Server/+");
  }
  
  else if (connectionListener && (preState != client.state())) {
    //If MQTT connection is failed then print the reason on the Serial Uart
    preState = client.state();
    connectionListener(client.connected());
    printCause(preState);
    char because[1024];
    wifiClient.lastError(because, 1024);
    Serial.printf("\tWifi Client Secure is %s because %s\n", wifiClient.connected() ? "Connected" : "Disconnected", because);
  }
}

/**
* Connects to Mosquitto server
* server   URL/IP address of server
* port     MQTT server's port (8082 or 8083)
* mac      MAC address of connecting device
* id       User ID (Gmail or Mobile no)
* password password to connect to the MQTT server
*/
void MQTT::Connect(const char* server, uint16_t port, const char* mac, const char* id, const char* password) {
  client.setServer(server, port);
  strcpy(this->mac, mac);
  strcpy(this->id, id);
  strcpy(this->password, password);

  //Will topic to notify server and user that this device is turned offline
  sprintf(willTopic, "Fanap/%s/IamGone", this->id);
  _Connect();
}

/**
* Sends data via MQTT 
* command Part of Command
* data    Part of payload
*/
void MQTT::send(const char* command, const char* data) {
  char topic[1024];
  sprintf(topic, "%s/%s", topicSend, command);
  client.publish(topic, data);
}

/**
* Sets MQTT connection states callback
* (bool isConnected)
*/
void MQTT::setOnMQTTConnection(ConnectionListener listener) {  connectionListener = listener; }

/**
* Do mqtt jobs and fire evens
*/
void MQTT::loop() {
  if (startMQTT) {
    bool connected = client.loop();  
    if (preConnect != connected) {

      //If connection status is jchanged then print the cause in Serial for debugging
      preConnect = connected;
      preState = client.state();
      connectionListener(preConnect);
      printCause(preState);
    }
  }
}
