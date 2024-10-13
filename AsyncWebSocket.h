#include "llc_array_pod.h"

#include <ESPAsyncWebServer.h>
#include "AsyncWebSynchronization.h"

#ifdef LLC_ESP32
#   include <AsyncTCP.h>
#elif defined(LLC_ESP8266)
#   include <ESPAsyncTCP.h>
#   include <Hash.h>
#   ifdef CRYPTO_HASH_h // include Hash.h from espressif framework if the first include was from the crypto library
#       include <../src/Hash.h>
#   endif
#endif

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
#ifndef ASYNCWEBSOCKET_H_
#define ASYNCWEBSOCKET_H_

namespace llc
{
#ifdef LLC_ESP32
    stxp uint8_t     SSE_MAX_QUEUED_MESSAGES     = 32;
    stxp uint8_t     DEFAULT_MAX_SSE_CLIENTS     = 8;
#elif defined(LLC_ESP8266)
    stxp uint8_t     SSE_MAX_QUEUED_MESSAGES     = 8;
    stxp uint8_t     DEFAULT_MAX_SSE_CLIENTS     = 4;
#endif
    class SAWSocket;
    class SAWSocketResponse;
    class SAWSocketClient;
    class SAWSocketControl;

    struct SAWSocketFrameInfo {
        uint8_t   message_opcode  = {}; // Message type as defined by enum AwsFrameType. Note: Applications will only see WS_TEXT and WS_BINARY. All other types are handled by the library. */
        uint32_t  num             = {}; // Frame number of a fragmented message. */
        uint8_t   final           = {}; // Is this the last frame in a fragmented message ?*/
        uint8_t   masked          = {}; // Is this frame masked? */
        uint8_t   opcode          = {}; // Message type as defined by enum AwsFrameType. This value is the same as message_opcode for non-fragmented messages, but may also be WS_CONTINUATION in a fragmented message.
        uint64_t  len             = {}; // Length of the current frame. This equals the total length of the message if num == 0 && final == true */
        uint8_t   mask  [4]       = {};    // Mask key */
        uint64_t  index           = {}; // Offset of the data inside the current frame. */
    };

    typedef enum { WS_DISCONNECTED, WS_CONNECTED, WS_DISCONNECTING } AwsClientStatus;
    typedef enum { WS_CONTINUATION, WS_TEXT, WS_BINARY, WS_DISCONNECT = 0x08, WS_PING, WS_PONG } AwsFrameType;
    typedef enum { WS_MSG_SENDING, WS_MSG_SENT, WS_MSG_ERROR } AwsMessageStatus;
    typedef enum { WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_PONG, WS_EVT_ERROR, WS_EVT_DATA } AwsEventType;

    class SAWSocketMessageBuffer {
        uint8_t             * _data                       = {};
        size_t              _len                          = {};
        bool                _lock                         = {};
        uint32_t            _count                        = {};
    public:                 ~SAWSocketMessageBuffer  ();
        inxp             SAWSocketMessageBuffer   ()  = default;
                            SAWSocketMessageBuffer   (size_t size);
                            SAWSocketMessageBuffer   (uint8_t * data, size_t size);
                            SAWSocketMessageBuffer   (const SAWSocketMessageBuffer &);
                            SAWSocketMessageBuffer   (SAWSocketMessageBuffer &&);

        inline  void        operator++                    (int i)         { (void)i; _count++; }
        inline  void        operator--                    (int i)         { (void)i; if (_count > 0) { _count--; } ;  }
        inline  void        lock                          ()              { _lock = true; }
        inline  void        unlock                        ()              { _lock = false; }
        inline  uint8_t*    get                           ()      const   { return _data; }
        inline  size_t      length                        ()      const   { return _len; }
        inline  uint32_t    count                         ()      const   { return _count; }
        inline  bool        canDelete                     ()      const   { return (0 == _count && false == _lock); }

        bool                reserve                       (size_t size);

        friend              SAWSocket;
    };

    class SAWSocketMessage {
    prtctd:
        uint8_t _opcode;
        bool _mask;
        AwsMessageStatus _status;
    public:
        SAWSocketMessage():_opcode(WS_TEXT),_mask(false),_status(WS_MSG_ERROR){}
        virtual ~SAWSocketMessage(){}
        virtual void ack(size_t len __attribute__((unused)), uint32_t time __attribute__((unused))){}
        virtual size_t send(AsyncClient *client __attribute__((unused))){ return 0; }
        virtual bool finished(){ return _status != WS_MSG_SENDING; }
        virtual bool betweenFrames() const { return false; }
    };

    class SAWSocketBasicMessage : public SAWSocketMessage {
        size_t                        _len                        = {};
        size_t                        _sent                       = {};
        size_t                        _ack                        = {};
        size_t                        _acked                      = {};
        uint8_t                       * _data                     = {};
    public: virtual                   ~SAWSocketBasicMessage () override;
                                      SAWSocketBasicMessage  (const char * data, size_t len, uint8_t opcode=WS_TEXT, bool mask=false);
                                      SAWSocketBasicMessage  (uint8_t opcode=WS_TEXT, bool mask=false);
        virtual bool                  betweenFrames               ()                          const override { return _acked == _ack; }
        virtual void                  ack                         (size_t len, uint32_t time)       override;
        virtual size_t                send                        (AsyncClient *client)             override;
    };

    class SAWSocketMultiMessage: public SAWSocketMessage {
        uint8_t                       * _data                     = {};
        size_t                        _len                        = {};
        size_t                        _sent                       = {};
        size_t                        _ack                        = {};
        size_t                        _acked                      = {};
        SAWSocketMessageBuffer   * _WSbuffer                 = {};
    public: virtual                   ~SAWSocketMultiMessage () override;
                                      SAWSocketMultiMessage  (SAWSocketMessageBuffer * buffer, uint8_t opcode=WS_TEXT, bool mask=false);

        virtual bool                  betweenFrames               () const override { return _acked == _ack; }
        virtual void                  ack                         (size_t len, uint32_t time) override ;
        virtual size_t                send                        (AsyncClient *client) override ;
    };

    class SAWSocketClient {
        typedef SAWSocketControl TAWSControl;
        typedef SAWSocketMessage TAWSMessage;
        AsyncClient                   * _client             = {};
        SAWSocket                * _server             = {};
        uint32_t                      _clientId             = {};
        AwsClientStatus               _status               = {};
        LinkedList<TAWSControl*>      _controlQueue         ;
        LinkedList<TAWSMessage*>      _messageQueue         ;
        uint8_t                       _pstate               = {};
        SAWSocketFrameInfo                  _pinfo                = {};
        uint32_t                      _lastMessageTime      = {};
        uint32_t                      _keepAlivePeriod      = {};

        void                          _queueControl         (TAWSControl * controlMessage);
        void                          _queueMessage         (TAWSMessage * dataMessage);
        void                          _runQueue             ();

    public: void                      * _tempObject         = {};

                                      ~SAWSocketClient ();
                                      SAWSocketClient  (SAWServerRequest *request, SAWSocket *server);
        //client id increments for the given server
        uint32_t                      id                    ()  const   { return _clientId; }
        AwsClientStatus               status                ()          { return _status; }
        AsyncClient*                  client                ()          { return _client; }
        SAWSocket*               server                ()          { return _server; }
        SAWSocketFrameInfo const &          pinfo                 ()  const   { return _pinfo; }
        IPAddress                     remoteIP              ();
        uint16_t                      remotePort            ();
        //control frames
        void                          close                 (uint16_t code = 0, const char * message = 0);
        void                          ping                  (uint8_t * data = 0, size_t len = 0);
        //set auto-ping period in seconds. disabled if zero (default)
        inline  void                  keepAlivePeriod       (uint16_t seconds)  { _keepAlivePeriod = seconds * 1000; }
        inline  uint16_t              keepAlivePeriod       ()    const         { return (uint16_t)(_keepAlivePeriod / 1000); }
        inline  bool                  canSend               ()    const         { return _messageQueue.length() < WS_MAX_QUEUED_MESSAGES; }
        //data packets
        void                          message               (SAWSocketMessage *message){ _queueMessage(message); }
        bool                          queueIsFull           ();
#ifndef LLC_ESP32
        size_t                        printf_P              (PGM_P formatP, ...)  __attribute__ ((format (printf, 2, 3)));
        void                          text                  (const __FlashStringHelper *data);
        void                          binary                (const __FlashStringHelper *data, size_t len);
#endif
        size_t                        printf                (const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
        void                          text                  (const char * message, size_t len);
        void                          text                  (const char * message);
        void                          text                  (uint8_t * message, size_t len);
        void                          text                  (char * message);
        void                          text                  (const String & message);
        void                          text                  (SAWSocketMessageBuffer *buffer);
        void                          binary                (const char * message, size_t len);
        void                          binary                (const char * message);
        void                          binary                (uint8_t * message, size_t len);
        void                          binary                (char * message);
        void                          binary                (const String &message);
        void                          binary                (SAWSocketMessageBuffer *buffer);
        //system callbacks (do not call)
        void                          _onAck                (size_t len, uint32_t time);
        void                          _onError              (int8_t);
        void                          _onPoll               ();
        void                          _onTimeout            (uint32_t time);
        void                          _onDisconnect         ();
        void                          _onData               (void *pbuf, size_t plen);
    };

    typedef std::function<void(SAWSocket * server, SAWSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)> AwsEventHandler;

    //WebServer Handler implementation that plays the role of a socket server
    class SAWSocket : public AsyncWebHandler {
    public:
        typedef SAWSocketClient  TAWSClient;
        typedef LinkedList<TAWSClient*> TAWSClientList;
    privte:
        String _url;
        TAWSClientList        _clients;
        uint32_t _cNextId;
        AwsEventHandler _eventHandler;
        bool _enabled;
        AsyncWebLock _lock;

    public:
        SAWSocket(const String& url);
        ~SAWSocket();
        const char * url() const { return _url.c_str(); }
        void enable(bool e){ _enabled = e; }
        bool enabled() const { return _enabled; }
        bool availableForWriteAll();
        bool availableForWrite(uint32_t id);

        size_t count() const;
        TAWSClient * client(uint32_t id);
        bool hasClient(uint32_t id){ return client(id) != NULL; }

        void close(uint32_t id, uint16_t code=0, const char * message=NULL);
        void closeAll(uint16_t code=0, const char * message=NULL);
        void cleanupClients(uint16_t maxClients = DEFAULT_MAX_WS_CLIENTS);

        void ping(uint32_t id, uint8_t *data=NULL, size_t len=0);
        void pingAll(uint8_t *data=NULL, size_t len=0); //  done

        void text(uint32_t id, const char * message, size_t len);
        void text(uint32_t id, const char * message);
        void text(uint32_t id, uint8_t * message, size_t len);
        void text(uint32_t id, char * message);
        void text(uint32_t id, const String &message);

        void textAll(const char * message, size_t len);
        void textAll(const char * message);
        void textAll(uint8_t * message, size_t len);
        void textAll(char * message);
        void textAll(const String &message);
        void textAll(SAWSocketMessageBuffer * buffer);

        void binary(uint32_t id, const char * message, size_t len);
        void binary(uint32_t id, const char * message);
        void binary(uint32_t id, uint8_t * message, size_t len);
        void binary(uint32_t id, char * message);
        void binary(uint32_t id, const String &message);

        void binaryAll(const char * message, size_t len);
        void binaryAll(const char * message);
        void binaryAll(uint8_t * message, size_t len);
        void binaryAll(char * message);
        void binaryAll(const String &message);
        void binaryAll(SAWSocketMessageBuffer * buffer);

        void message(uint32_t id, SAWSocketMessage *message);
        void messageAll(SAWSocketMultiMessage *message);

        size_t printf(uint32_t id, const char *format, ...)  __attribute__ ((format (printf, 3, 4)));
        size_t printfAll(const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
#ifndef LLC_ESP32
        void text         (uint32_t id, const __FlashStringHelper *message);
        void textAll      (const __FlashStringHelper *message); //  need to convert
        void binary       (uint32_t id, const __FlashStringHelper *message, size_t len);
        void binaryAll    (const __FlashStringHelper *message, size_t len);
        size_t printf_P(uint32_t id, PGM_P formatP, ...)  __attribute__ ((format (printf, 3, 4)));
#endif
        size_t printfAll_P(PGM_P formatP, ...)  __attribute__ ((format (printf, 2, 3)));
        void onEvent(AwsEventHandler handler){ _eventHandler = handler; }
        //system callbacks (do not call)
        uint32_t _getNextId(){ return _cNextId++; }
        void _addClient(SAWSocketClient * client);
        void _handleDisconnect(SAWSocketClient * client);
        void _handleEvent(SAWSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
        virtual bool canHandle(SAWServerRequest *request) override final;
        virtual void handleRequest(SAWServerRequest *request) override final;


        //  messagebuffer functions/objects.
        SAWSocketMessageBuffer * makeBuffer(size_t size = 0);
        SAWSocketMessageBuffer * makeBuffer(uint8_t * data, size_t size);
        LinkedList<SAWSocketMessageBuffer *> _buffers;
        void _cleanBuffers();

        SAWSocketClientLinkedList getClients() const;
    };

    //WebServer response to authenticate the socket and detach the tcp client from the web server request
    class SAWSocketResponse: public SAWServerResponse {
    privte:
        String _content;
        SAWSocket *_server;
    public:
        SAWSocketResponse(const String& key, SAWSocket *server);
        void _respond(SAWServerRequest *request);
        size_t _ack(SAWServerRequest *request, size_t len, uint32_t time);
        bool _sourceValid() const { return true; }
    };
} // namespace
#endif /* ASYNCWEBSOCKET_H_ */
