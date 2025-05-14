#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#define COMMAND 1
#define LENGTH  2
#define PAYLOAD 3
#define SEPARATOR 10

#define HOTSPOT 1

class Wifi {
  private:
    typedef void (*ConnectionListener) (bool isConnected);          //Callback for connection states
    typedef void (*DataListener)       (char* command, uint8_t* payload, uint16_t size);//Callback for data received

    //String chars for different wifi states
    const char* reasons[9] PROGMEM = {
        "No Shield",
        "Idle",
        "No SSID available",
        "Scan is completed",
        "Connected",
        "Connection Failed",
        "Connection is lost",
        "Disconnected",
        "Unknown status"
      };

    uint32_t lastTime;
    uint16_t globalIndex, count, len;
    uint8_t 
      payload[2000], 
      step, 
      status, 
      mode, 
      current_received, 
      wifiReboot;

    char command[50];
    bool initialized, inReading, doStop;
    WiFiServer server;
    WiFiClient client;
    IPAddress remote;

    ConnectionListener connectionListener;
    DataListener       dataListener;

    const char* getCause(uint8_t status) {
      switch (status) {
        case WL_NO_SHIELD:       return reasons[0]; break;
        case WL_IDLE_STATUS:     return reasons[1]; break;
        case WL_NO_SSID_AVAIL:   return reasons[2]; break;
        case WL_SCAN_COMPLETED:  return reasons[3]; break;
        case WL_CONNECTED:       return reasons[4]; break;
        case WL_CONNECT_FAILED:  return reasons[5]; break;
        case WL_CONNECTION_LOST: return reasons[6]; break;
        case WL_DISCONNECTED:    return reasons[7]; break;
        default:                 return reasons[8];
      }
    }

    //Read part of Command in wifi received
    bool readCommand() {
      bool result = false;
      if (step == COMMAND) {
        if (current_received == SEPARATOR) {
          command[len] = NULL;
          step         = LENGTH;
          globalIndex  = 0;
          result       = true;
        }
        else
          command[len++] = current_received;
      }
      return result;
    }

    //Read Length of payload in wifi received
    bool readLength() {
      bool result = false;
      if (step == LENGTH) {
        if (current_received == SEPARATOR) {
          payload[globalIndex] = NULL;
          step                 = PAYLOAD;
          len = atoi((char*)payload);

          //If no payload is available
          if ((len == 0) && (globalIndex > 0)) {
            inReading = false;
            if (dataListener != NULL)
              dataListener(command, payload, globalIndex);
            step = COMMAND;
            len  = 0;
          }
          globalIndex = 0;
          result      = true;
        }

        else
          payload[globalIndex++] = current_received;
      }
      return result;
    }

    //Read Payload of wifi received
    bool readPayload() {
      bool result = false;
      if (step == PAYLOAD) {
        if (len > 0) {
          payload[globalIndex++] = current_received;
          if (globalIndex == len) {
            inReading = false;
            if (dataListener != NULL) 
              dataListener(command, payload, len);
            //Reset the parameters to read another command.len.payload
            step        = COMMAND;
            globalIndex = 0;
            len         = 0;
            result      = true;
          }
        }
        
        else {
          inReading = false;
          if (dataListener != NULL)
            dataListener(command, payload, len);
          step        = COMMAND;
          len         = 0;
          globalIndex = 0;
          result      = true;
        }
      }
      return result;
    }
  
  public:
  
    Wifi():server(5000) {
      step        = COMMAND;
      len         = 0;
      globalIndex = 0;
      mode        = 1;
      initialized = false;
      inReading   = false;
      doStop      = false;
      wifiReboot  = 15;
      lastTime    = 1;
      remote      = IPAddress(0,0,0,0);
    }

    /**
    * writes MAC address in given string variable
    */
    void getMAC(char* mac) {
      uint8_t MAC[6];
      WiFi.macAddress(MAC);
      sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
        MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
    }

    /**
    * initialization of WiFi helper with given informations
    * ssid name of WiFi modem which should be connect to or Hotspot will be
    * password password of remote Wifi which should be connect to
    * _mode mode of WiFi should be. 1=Hotspot , 2=Client
    */
    void init(const char* ssid, const char* password, uint8_t _mode) {
      mode = _mode;
      char name[14];
      uint8_t MAC[6];
      WiFi.macAddress(MAC);

      //SSID of Hotspot mode
      sprintf(name, "Fanap_%02X%02X%02X", MAC[3], MAC[4], MAC[5]);

      if (mode == 1)
        WiFi.softAP(name);
      else {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
      }

      server.begin();      
      initialized = true;
      len         = 0;
    }

    /**
    * Start of receiving data from WiFi
    */
    void startServer() {
      server.begin();
      initialized = true;
    }

    /**
    * Sends a data in form of Command.Length.Payload 
    */
    void send(const char* command, const char* payload) {
      if (initialized && (remote != IPAddress(0,0,0,0) /* means there is at least one contact to send data*/)) {
        if (!client.connected()) {
          if (!client.connect(remote, 5000)) {
            Serial.println("Can't send data over TCP");
            remote = IPAddress(0,0,0,0);
            return;
          }
        }

        //println send byte of 10 or SEPARATOR in end of data
        client.println(command);
        client.println(payload);
        doStop = true;
      }
        
    }

    void stop() { client.stop(); }

    void setOnWifiConnection (ConnectionListener listener) { connectionListener = listener; }    
    void setOnDataReceived   (DataListener listener)       { dataListener = listener; }    
  
    void loop() {
      if (mode > HOTSPOT)
        if (WiFi.status() != status) { //WiFi status is changed
          status = WiFi.status();
          if (status == WL_CONNECTED) {
            //Fires callback for successful connection
            connectionListener(true);
            lastTime = 0;
          }
          
          else if (status != WL_NO_SHIELD) { //WiFi is not connected because of...
            connectionListener(false);
            if (lastTime == 0)
              lastTime = millis(); //Measures time of WiFi disconnection
          }
        }
      
      //Check every tick is there a connection to get data or not?
      if (client.connected()) {
        remote = client.remoteIP();
        while (client.available()) {
          inReading = true;
          current_received = client.read();

          //process the received data in functions to complete the data partitions
          //data partitions => Command[10]Length[10]Payload
          if (readCommand()) continue;
          if (readLength())  continue;
          if (readPayload()) break;
        }

        if (!inReading && doStop) {
          //If not in reading data mode and the data is send completly, stop the client to be closed
          // on other side
          doStop = false;
          client.stop();
        }
      }
      
      else
        client = server.available();
    }
};
