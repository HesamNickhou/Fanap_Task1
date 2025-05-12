#ifndef FILE_h
#define FILE_h

#include <Arduino.h>
#include "SPIFFS.h"

class myFile {
  public:
    myFile() {}
    
    /**
    * @brief Given file name and buffer, reads data untill reached given size
    * @param fileName Name of file to be read
    * @param buffer Memory space to read data into
    * @param size Count of bytes to be read
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
    * @brief Writes data into the given file name
    * @param fileName Name of file which should be write into
    * @param data Buffer of data should be write into the file
    */
    void fWrite(const char* fileName, const char* data) {
      File file = SPIFFS.open(fileName, FILE_WRITE);
      file.print(data);
      file.close();
    }
    
    /**
    * @brief Deletes a file
    * @param fileName Name of file
    */
    void fDelete(const char* fileName) {
      SPIFFS.remove(fileName);
    }
};

#endif
