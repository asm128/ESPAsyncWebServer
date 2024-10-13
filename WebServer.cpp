/*
  Asynchronous WebServer library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

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
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"

bool ON_STA_FILTER(SAWServerRequest *request) {
  return WiFi.localIP() == request->client()->localIP();
}

bool ON_AP_FILTER(SAWServerRequest *request) {
  return WiFi.localIP() != request->client()->localIP();
}


SAWServer::SAWServer(uint16_t port)
  : _server(port)
  , _rewrites(LinkedList<AsyncWebRewrite*>([](AsyncWebRewrite* r){ delete r; }))
  , _handlers(LinkedList<AsyncWebHandler*>([](AsyncWebHandler* h){ delete h; }))
{
  _catchAllHandler = new AsyncCallbackWebHandler();
  if(_catchAllHandler == NULL)
    return;
  _server.onClient([](void *s, AsyncClient* c){
    if(c == NULL)
      return;
    c->setRxTimeout(3);
    SAWServerRequest *r = new SAWServerRequest((SAWServer*)s, c);
    if(r == NULL){
      c->close(true);
      c->free();
      delete c;
    }
  }, this);
}

SAWServer::~SAWServer(){
  reset();  
  end();
  if(_catchAllHandler) 
    delete _catchAllHandler;
}
void              SAWServer::_attachHandler    (SAWServerRequest * request)     {
    for(const auto& h: _handlers)
        if (h->filter(request) && h->canHandle(request)){
            request->setHandler(h);
            return;
        }
    request->addInterestingHeader("ANY");
    request->setHandler(_catchAllHandler);
}


AsyncCallbackWebHandler& SAWServer::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest, ArUploadHandlerFunction onUpload, ArBodyHandlerFunction onBody){
  AsyncCallbackWebHandler* handler = new AsyncCallbackWebHandler();
  handler->setUri(uri);
  handler->setMethod(method);
  handler->onRequest(onRequest);
  handler->onUpload(onUpload);
  handler->onBody(onBody);
  addHandler(handler);
  return *handler;
}

AsyncCallbackWebHandler& SAWServer::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest, ArUploadHandlerFunction onUpload){
  AsyncCallbackWebHandler* handler = new AsyncCallbackWebHandler();
  handler->setUri(uri);
  handler->setMethod(method);
  handler->onRequest(onRequest);
  handler->onUpload(onUpload);
  addHandler(handler);
  return *handler;
}

AsyncCallbackWebHandler& SAWServer::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest){
  AsyncCallbackWebHandler* handler = new AsyncCallbackWebHandler();
  handler->setUri(uri);
  handler->setMethod(method);
  handler->onRequest(onRequest);
  addHandler(handler);
  return *handler;
}

AsyncCallbackWebHandler& SAWServer::on(const char* uri, ArRequestHandlerFunction onRequest){
  AsyncCallbackWebHandler* handler = new AsyncCallbackWebHandler();
  handler->setUri(uri);
  handler->onRequest(onRequest);
  addHandler(handler);
  return *handler;
}

AsyncStaticWebHandler& SAWServer::serveStatic(const char* uri, fs::FS& fs, const char* path, const char* cache_control){
  AsyncStaticWebHandler* handler = new AsyncStaticWebHandler(uri, fs, path, cache_control);
  addHandler(handler);
  return *handler;
}
void SAWServer::reset            (){
  _rewrites.free();
  _handlers.free();
  if (_catchAllHandler){
    _catchAllHandler->onRequest({});
    _catchAllHandler->onUpload({});
    _catchAllHandler->onBody({});
  }
}

