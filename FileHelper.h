#ifndef FILE_h
#define FILE_h

#include <Arduino.h>
#include "SPIFFS.h"

class FileHelper {
  public:
    FileHelper() {}
    
    /**
    * Read from (fileName) into (buffer) in amount of (size) bytes
    */
    bool fRead(const char* fileName, char* buffer, uint16_t size) {
      uint16_t counter = 0;
      buffer[0] = '\0'; //Reset the given buffer
      if (!SPIFFS.exists(fileName))
        return false;
      else {
        File file = SPIFFS.open(fileName, FILE_READ);
        if (file) {
          while (file.available() && (counter < size))
            buffer[counter++] = char(file.read());
          buffer[counter] = NULL;
          file.close();
          return true;
        }
        else
          return false;
      }
    }
    
    /**
    * Writes data from (data) into the (fileName)
    */
    void fWrite(const char* fileName, const char* data) {
      File file = SPIFFS.open(fileName, FILE_WRITE);
      file.print(data);
      file.close();
    }
    
    /**
    * Deletes a file
    */
    void fDelete(const char* fileName) {
      SPIFFS.remove(fileName);
    }
};

#endif
