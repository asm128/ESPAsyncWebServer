#include "llc_array_pod.h"
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
#ifdef LLC_ESP32
#   include <AsyncTCP.h>
#elif defined(LLC_ESP8266)
#   include <ESPAsyncTCP.h>
#endif

#include "ESPAsyncWebServer.h"
#include "AsyncWebSynchronization.h"

#ifdef LLC_ESP8266
#   include <Hash.h>
#   ifdef CRYPTO_HASH_h // include Hash.h from espressif framework if the first include was from the crypto library
#       include <../src/Hash.h>
#   endif
#endif


#ifndef ASYNCEVENTSOURCE_H_
#define ASYNCEVENTSOURCE_H_

namespace llc
{
    String              generateEventMessage        (const char *message, const char *event, uint32_t id, uint32_t reconnect);
#ifdef LLC_ESP32
    stxp uint8_t     SSE_MAX_QUEUED_MESSAGES     = 32;
    stxp uint8_t     DEFAULT_MAX_SSE_CLIENTS     = 8;
#elif defined(LLC_ESP8266)
    stxp uint8_t     SSE_MAX_QUEUED_MESSAGES     = 8;
    stxp uint8_t     DEFAULT_MAX_SSE_CLIENTS     = 4;
#endif
    class AsyncEventSourceMessage {
        au0_t                 _data                       = {};
        uint32_t            _sent                       = {};
        uint32_t            _acked                      = {};
        //size_t            _ack;
    public:
                            AsyncEventSourceMessage     (vcu0_t data) : _data{data} {}

        inxp bool        finished                    ()      const   { return _acked == _data.size(); }
        inxp bool        sent                        ()      const   { return _sent  == _data.size(); }
        size_t              ack                         (size_t len, uint32_t /*time*/) {
            if(_acked + len <= _data.size()) {  // If the whole message is not acked...
                _acked += len;
                return 0; // Return that no extra bytes left.
            }
            const size_t extra = _acked + len - _data.size();
            _acked = _data.size();
            return extra; // Return the number of extra bytes acked (they will be carried on to the next message)
        }
        size_t              send                        (AsyncClient *client) {
            const size_t        len         = _data.size() - _sent;
            if(client->space() < len)
                return 0;
            size_t sent = client->add((const char *)_data.begin(), len);
            if(client->canSend())
                client->send();
            _sent += sent;
            return sent; 
        }
    };
    tplt_T
    class AsyncEventSourceClient {
        typedef T                           TAsyncEventSource;
        typedef AsyncEventSourceMessage     TSourceMessage;
        typedef LinkedList<TSourceMessage*> TMessageList;
        AsyncClient              * _client              = {};
        TAsyncEventSource        * _server              = {};
        TMessageList             _messageQueue          ;
        uint32_t                 _lastId                = {};

        void                    _queueMessage           (AsyncEventSourceMessage * dataMessage){
            if(0 == dataMessage)
                return;
            if(false == connected()){
                delete dataMessage;
                return;
            }
            if(_messageQueue.length() < SSE_MAX_QUEUED_MESSAGES)
                _messageQueue.add(dataMessage);
            else {
                ets_printf("ERROR: Too many messages queued\n");
                delete dataMessage;
            }
            if(_client->canSend())
                _runQueue();
        }
        void                    _runQueue               () {
            while(false == _messageQueue.isEmpty() && _messageQueue.front()->finished())
                _messageQueue.remove(_messageQueue.front());
            for(auto i = _messageQueue.begin(); i != _messageQueue.end(); ++i) {
                if(false == (*i)->sent())
                    (*i)->send(_client);
            }
        }
    public:
                                ~AsyncEventSourceClient ()                                      { _messageQueue.free(); close(); }
                                AsyncEventSourceClient  (SAWServerRequest * request, TAsyncEventSource * server)
            : _messageQueue(LinkedList<AsyncEventSourceMessage *>([](AsyncEventSourceMessage *m){ delete  m; })) {
            _client = request->client();
            _server = server;
            _lastId = 0;
            if(request->hasHeader("Last-Event-ID"))
                _lastId = atoi(request->getHeader("Last-Event-ID")->value().c_str());
                
            _client->setRxTimeout(0);
            _client->onError        ({}, {});
            _client->onAck          ([](void *r, AsyncClient* c, size_t len, uint32_t time){ (void)c; ((AsyncEventSourceClient*)(r))->_onAck(len, time); }, this);
            _client->onPoll         ([](void *r, AsyncClient* c){ (void)c; ((AsyncEventSourceClient*)(r))->_onPoll(); }, this);
            _client->onData         ({}, {});
            _client->onTimeout      ([this](void *r, AsyncClient* c __attribute__((unused)), uint32_t time){ ((AsyncEventSourceClient*)(r))->_onTimeout(time); }, this);
            _client->onDisconnect   ([this](void *r, AsyncClient* c){ ((AsyncEventSourceClient*)(r))->_onDisconnect(); delete c; }, this);

            _server->_addClient(this);
            delete request;
        }
        inline  bool            connected               ()                          const       { return _client && _client->connected(); }
        inline  AsyncClient*    client                  ()                          const       { return _client; }
        inline  uint32_t        lastId                  ()                          const       { return _lastId; }
        inline  uint32_t        packetsWaiting          ()                          const       { return (uint32_t)_messageQueue.length(); }
        inline  void            close                   ()                                      { if(_client) _client->close(); }
        inline  void            write                   (const char * message, size_t len)      { _queueMessage(new AsyncEventSourceMessage({(const uint8_t*)message, len})); }
        void                    send                    (const char * message, const char * event = 0, uint32_t id = 0, uint32_t reconnect = 0) {
            String ev = generateEventMessage(message, event, id, reconnect);
            _queueMessage(new AsyncEventSourceMessage({(const uint8_t*)ev.begin(), (uint32_t)ev.length()}));
        }
        inline  void            _onTimeout              (uint32_t)                              { _client->close(true); }
        inline  void            _onDisconnect           ()                                      { _client = {}; _server->_handleDisconnect(this); }
        inline  void            _onPoll                 ()                                      { if(false == _messageQueue.isEmpty()) _runQueue(); }
        void                    _onAck                  (uint32_t len, uint32_t time){
            while(len && false == _messageQueue.isEmpty()){
                len   = _messageQueue.front()->ack(len, time);
                if(_messageQueue.front()->finished())
                    _messageQueue.remove(_messageQueue.front());
            }
            _runQueue();
        }
    };
    tplt_T
    class AsyncEventSourceResponse : public SAWServerResponse {
        typedef             T                           TEventSource;
        String              _content                    = {};
        TEventSource        * _server                  = {};
    public:
                            AsyncEventSourceResponse    (TEventSource * server) {
            _server             = server;
            _code               = 200;
            _contentType        = "text/event-stream";
            _sendContentLength  = false;
            addHeader("Cache-Control", "no-cache");
            addHeader("Connection","keep-alive");
        }
        inxp bool        _sourceValid                ()                              const { return true; }
        inline  size_t      _ack                        (SAWServerRequest * request, size_t len, uint32_t /*time*/) {
            if(len)
                new AsyncEventSourceClient<T>(request, _server);
            return 0;
        }
        void                _respond                    (SAWServerRequest * request) {
            String                  out         = _assembleHead(request->version());
            request->client()->write(out.c_str(), _headLength);
            _state              = RESPONSE_WAIT_ACK;
        }
    };
    class AsyncEventSource : public AsyncWebHandler {
        tplt_T  using                       TSourceClient       = AsyncEventSourceClient<T>;
        typedef AsyncEventSource            TEventSource;
        typedef TSourceClient<TEventSource> TClient;
        typedef function<void(TClient*)>    FEventHandler;

        asc_t                               _url                = {};
        LinkedList<TClient*>                _clients;
    public:
        FEventHandler                       onConnect                       = {};

                                ~AsyncEventSource               ()                                                  { close(); }
                                AsyncEventSource                (const String & url)
            : _url{vcsc_t{url.begin(), url.length()}}, _clients(LinkedList<AsyncEventSourceClient<AsyncEventSource>*>([](AsyncEventSourceClient<AsyncEventSource> *c){ delete c; })) {}

        ndinlne vcs             url                             ()                                  const           { return _url.cc(); }
        ndinlne size_t          count                           ()                                  const           { return _clients.count_if([](AsyncEventSourceClient<AsyncEventSource> *c){ return c->connected(); }); }
        inline  void            _handleDisconnect               (AsyncEventSourceClient<AsyncEventSource> * client) { _clients.remove(client); }
        inline  void            _addClient                      (AsyncEventSourceClient<AsyncEventSource> * client) { _clients.add(client); if(onConnect) onConnect(client); }
        inline  void            close                           ()                                                  { for(const auto & c : _clients) if(c->connected()) c->close();}
        virtual void            handleRequest                   (SAWServerRequest * request) override final    {
            if((_username.length() && _password.length()) && false == request->authenticate(_username.c_str(), _password.c_str()))
                request->requestAuthentication();
            else
                request->send(new AsyncEventSourceResponse<AsyncEventSource>(this));
        }
        virtual bool            canHandle                       (SAWServerRequest * request) override final {
            if(request->method() != HTTP_GET || vcsc_t{request->url().begin(), request->url().length()} == _url.cc())
                return false;
            request->addInterestingHeader("Last-Event-ID");
            return true;
        }
        void                    send                            (const char * message, const char * event = 0, uint32_t id = 0, uint32_t reconnect = 0) {
            String ev = generateEventMessage(message, event, id, reconnect);
            for(const auto &c: _clients){
                if(c->connected())
                    c->write(ev.c_str(), ev.length());
            }
        }
        size_t                  avgPacketsWaiting               ()  const {
            if(_clients.isEmpty())
                return 0;

            size_t    aql               = 0;
            uint32_t  nConnectedClients = 0;

            for(const auto & c : _clients) {
                if(false == c->connected())
                    continue;
                aql     +=  c->packetsWaiting();
                ++nConnectedClients;
            }
        //    return aql / nConnectedClients;
            return ((aql) + (nConnectedClients/2))/(nConnectedClients); // round up
        }
    };
} // namespace

#endif // ASYNCEVENTSOURCE_H
