/*
  Asynchronous WebServer library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "Arduino.h"
#include "AsyncEventSource.h"
#ifdef ESP32
#if ESP_IDF_VERSION_MAJOR >= 5
#include "rom/ets_sys.h"
#endif
#endif

String llc::generateEventMessage(const char * message, const char * event, uint32_t id, uint32_t reconnect) {
    String ev = "";
    if(reconnect)
        ev += "retry: " + String(reconnect)+ "\r\n";
    if(id)
        ev += "id: " + String(id) + "\r\n";
    if(event)
        ev += "event: " + String(event) + "\r\n";
    if(0 == message)
        return ev;

    size_t  messageLen  = strlen(message);
    char    * lineStart = (char *)message;
    char    * lineEnd;
    do {
        char * nextN = strchr(lineStart, '\n');
        char * nextR = strchr(lineStart, '\r');
        if(0 == nextN && 0 == nextR){
            size_t        llen          = ((char *)message + messageLen) - lineStart;
            char          * ldata       = (char *)malloc(llen+1);
            if(ldata) {
                memcpy(ldata, lineStart, llen);
                ldata[llen] = 0;
                ev          += "data: " + String(ldata) + "\r\n\r\n";
                free(ldata);
            }
            lineStart   = (char *)message + messageLen;
            continue;
        } 
        char * nextLine = NULL;
        if(0 == nextN || 0 == nextR) {
            lineEnd   = nextN ? nextN : nextR;
            nextLine  = lineEnd + 1;
        }
        else {
            if(nextR < nextN) {
                lineEnd = nextR;
                nextLine = nextN + one_if(nextN == (lineEnd + 1));
            } else {
                lineEnd = nextN;
                nextLine = nextR + one_if(nextR == (lineEnd + 1));
            }
        }
        size_t  llen    = lineEnd - lineStart;
        char    * ldata = (char *)malloc(llen+1);
        if(ldata) {
            memcpy(ldata, lineStart, llen);
            ldata[llen] = 0;
            ev += "data: " + String(ldata) + "\r\n";
            free(ldata);
        }
        lineStart = nextLine;
        if(lineStart == ((char *)message + messageLen))
            ev += "\r\n";
    } while(lineStart < ((char *)message + messageLen));


    return ev;
}

// Client
