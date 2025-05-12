# Fanap_Task1
A public task project based on ESP32DevKit1


The firmware starts with two tasks, Main task and Connection Task.
Main task is for main jobs include checking the sensor values, responsing to commands and executing scenarios.

Connection task is for doing connections issues like WiFi, MQTT, TCP and ....

These tasks communicate with each others via some buffers(QueuHandle_t)

The device starts with a SSID name of Fanap_MAC in hotspot mode to receive the remote modem credentials to connect.
After receiving credentials, the module restarts and connect to modem, after establishing WiFi connections
continue to connect to the MQTT server, with given address, user id, password and ....

I assumed there is an application which connects to the module via WiFi, and send the remote WiFi modem credentials
to the device. There is some limitations to protect data in the server.
This configuration is done before in a real platform and I follow it approximately!

Requests are divided in two parts: Command and Payload.
The transferring data are divided in two parts consequently