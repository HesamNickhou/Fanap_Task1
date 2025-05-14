#include "TaskDataTransformer.h"

TaskDataTransformer::TaskDataTransformer() {
  step     = 1;
  counter  = 0;
  element  = NULL;
  sender   = NULL;
  receiver = NULL;
  debug    = false;
}

/*
There are two queues, for sending and receiving.
In therefore, a name is required to distinguish the transformers from each other
Callback is to notify the tasks for incoming data
*/
void TaskDataTransformer::setReceiveQueue(QueueHandle_t queue)    { this->receiver = queue; }
void TaskDataTransformer::setSendQueue(QueueHandle_t queue)       { this->sender   = queue; }
void TaskDataTransformer::setDataListener(DataReceived interface) { this->listener = interface; }
void TaskDataTransformer::setName(const char* taskName)           { strcpy(name, taskName); }

/**
* Sends data with unlimited size
*/
void TaskDataTransformer::sendData(const char* cmd, const char* pld) {
  
  //If debug flag is true then shows the transferring data in Serial output for debugging
  if (debug)
    Serial.printf("%s >> %s=%s\n", name, cmd, pld);

  char ch;
  for (uint16_t i=0; i<strlen(cmd); i++) {
    ch = cmd[i];
    xQueueSend(sender, &ch, portMAX_DELAY);
  }
  //Send a NULL in end of data to show the current part is finished
  ch = NULL;
  xQueueSend(sender, &ch, portMAX_DELAY);

  for (uint16_t i=0; i<strlen(pld); i++) {
    ch = pld[i];
    xQueueSend(sender, &ch, portMAX_DELAY);
  }
  //Send a NULL to show the end of part
  ch = NULL;
  xQueueSend(sender, &ch, portMAX_DELAY);
}

/**
* Sends data in given size
*/
void TaskDataTransformer::sendData(const char* cmd, const char* pld, uint16_t size) {
  
  //If debug flag is True then shows the transferring data in Serial output for debugging
  if (debug)
    Serial.printf("%s >> %s=%s\n", name, cmd, pld);

  char ch;
  for (uint16_t i=0; i<strlen(cmd); i++) {
    ch = cmd[i];
    xQueueSend(sender, &ch, portMAX_DELAY);
  }
  //Send a NULL in end of data to show the data is finished
  ch = NULL;
  xQueueSend(sender, &ch, portMAX_DELAY);

  for (uint16_t i=0; i<size; i++) {
    ch = pld[i];
    xQueueSend(sender, &ch, portMAX_DELAY);
  }
  //Send a NULL in end of data to show the data is finished
  ch = NULL;
  xQueueSend(sender, &ch, portMAX_DELAY);
}

/**
* Receive data and fires the callback
*/
void TaskDataTransformer::loop() {

  //No delay to receive data. It should be non-blocking loop
  if (xQueueReceive(receiver, &element, 0)) {
    
    //Reading part of Command
    if (step == 1)
      command[counter++] = element;

    //Reading part of Payload
    else if (step == 2)
      payload[counter++] = element;

    if (element == NULL) {
      if (step == 2 && listener != NULL) { //If two parts are read
        
        //If debug flag is True the show the data in Serial output for debugging
        if (debug)
          Serial.printf("%s << %s=%s\n", name, command, payload);

        listener(command, payload); //Fires the callback
      }

      //Turn the round to read another data
      step = 3 - step;
      counter = 0;
    }
  }
}
