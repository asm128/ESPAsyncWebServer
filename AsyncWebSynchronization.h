#include "llc_array_pod.h"

#include "ESPAsyncWebServer.h"

#ifndef ASYNCWEBSYNCHRONIZATION_H_
#define ASYNCWEBSYNCHRONIZATION_H_

namespace llc
{
  // Synchronisation is only available on ESP32, as the ESP8266 isn't using FreeRTOS by default
#ifdef LLC_ESP8266
    // This is the 8266 version of the Sync Lock which is currently unimplemented
    struct AsyncWebLock {
        inline  bool        lock                ()      { return false; }
        inline  void        unlock              ()      {}
    };
    struct AsyncWebLockGuard { AsyncWebLockGuard(const AsyncWebLock &){} };
#elif defined(LLC_ESP32)
    // This is the ESP32 version of the Sync Lock, using the FreeRTOS Semaphore
    class AsyncWebLock {
    prtctd: void            * _lockedBy         = {};
        SemaphoreHandle_t   _lock               = {};
    public:     inline      ~AsyncWebLock       ()      { vSemaphoreDelete(_lock); }
        inline              AsyncWebLock        ()      : _lock{xSemaphoreCreateBinary()} { xSemaphoreGive(_lock); }
        inline  void        unlock              ()      { _lockedBy = {}; xSemaphoreGive(_lock); }
        bool                lock                ()      {
            extern void         * pxCurrentTCB;
            if(_lockedBy == pxCurrentTCB)
                return false;
            xQueueSemaphoreTake(_lock, portMAX_DELAY);
            _lockedBy   = pxCurrentTCB;
            return true;
        }
    };
    class AsyncWebLockGuard {
    prtctd: AsyncWebLock    * const _lock       = 0;
    public:                 ~AsyncWebLockGuard  ()                        { if (_lock) _lock->unlock(); }
                            AsyncWebLockGuard   (const AsyncWebLock & l)  : _lock{l.lock() ? &l : 0;} {}
    };
#endif
} // namespace

#endif // ASYNCWEBSYNCHRONIZATION_H_
