#include <ESPAsyncWebServer.h>

#ifndef SPIFFSEditor_H_
#define SPIFFSEditor_H_

class SPIFFSEditor: public AsyncWebHandler {
  private:
    fs::FS _fs;
    String _username;
    String _password; 
    bool _authenticated;
    uint32_t _startTime;
  public:
#ifdef LLC_ESP32
    SPIFFSEditor(const fs::FS& fs, const String& username=String(), const String& password=String());
#else
    SPIFFSEditor(const String& username=String(), const String& password=String(), const fs::FS& fs=SPIFFS);
#endif
    virtual bool canHandle(SAWServerRequest *request) override final;
    virtual void handleRequest(SAWServerRequest *request) override final;
    virtual void handleUpload(SAWServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
    virtual bool isRequestHandlerTrivial() override final {return false;}
};

#endif
