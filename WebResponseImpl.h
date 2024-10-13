#include "llc_array_pod.h"

#include "ESPAsyncWebServer.h"

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
#ifndef ASYNCWEBSERVERRESPONSEIMPL_H_
#define ASYNCWEBSERVERRESPONSEIMPL_H_

// It is possible to restore these defines, but one can use _min and _max instead. Or std::min, std::max.
namespace llc
{
    class AsyncBasicResponse : public SAWServerResponse {
    prtctd: String              _content                = {};
    public:                     AsyncBasicResponse      (int code, const String & contentType = {}, const String & content = {});
        void                    _respond                (SAWServerRequest * request);
        size_t                  _ack                    (SAWServerRequest * request, size_t len, uint32_t time);
        inline  bool            _sourceValid            ()  const   { return true; }
    };
    class AsyncAbstractResponse : public SAWServerResponse {
    prtctd: String              _head;
        au0_t                     _cache; // Data is inserted into cache at begin(). This is inefficient with vector, but if we use some other container, we won't be able to access it as contiguous array of bytes when reading from it, so by gaining performance in one place, we'll lose it in another.
        AwsTemplateProcessor    _callback;
        size_t                  _fillBufferAndProcessTemplates  (uint8_t * buf, size_t maxLen);
        size_t                  _readDataFromCacheOrContent     (uint8_t * data, const size_t len);
    public:
                                AsyncAbstractResponse   (AwsTemplateProcessor callback = 0);
        void                    _respond                (SAWServerRequest * request);
        size_t                  _ack                    (SAWServerRequest * request, size_t len, uint32_t time);
        inline  bool            _sourceValid            ()                                    const { return false; }
        virtual size_t          _fillBuffer             (uint8_t * /*buf*/, size_t /*maxLen*/)      { return 0; }
    };
    class AsyncFileResponse : public AsyncAbstractResponse {
    prtctd: void                _setContentType         (const String& path);
        using File              = fs::File;
        using FS                = fs::FS;
        File                    _content;
        String                  _path;
    public:                     ~AsyncFileResponse      ();
                                AsyncFileResponse       (FS &fs, const String& path, const String& contentType=String(), bool download=false, AwsTemplateProcessor callback=nullptr);
                                AsyncFileResponse       (File content, const String& path, const String& contentType=String(), bool download=false, AwsTemplateProcessor callback=nullptr);
        inline  bool            _sourceValid            ()                                      const { return !!(_content); }
        virtual size_t          _fillBuffer             (uint8_t *buf, size_t maxLen) override;
    };
    class AsyncStreamResponse : public AsyncAbstractResponse {
    prtctd: Stream              * _content              = {};
    public:                     AsyncStreamResponse     (Stream &stream, const String& contentType, size_t len, AwsTemplateProcessor callback=nullptr);
        inline  bool            _sourceValid            ()                                      const { return !!(_content); }
        virtual size_t          _fillBuffer             (uint8_t * buf, size_t maxLen) override;
    };
    class AsyncCallbackResponse: public AsyncAbstractResponse {
    prtctd: AwsResponseFiller   _content                = {};
        size_t                  _filledLength           = {};
    public:                     AsyncCallbackResponse   (const String& contentType, size_t len, AwsResponseFiller callback, AwsTemplateProcessor templateCallback=nullptr);
        inline  bool            _sourceValid            ()                                      const { return !!(_content); }
        virtual size_t          _fillBuffer             (uint8_t * buf, size_t maxLen) override;
    };
    class AsyncChunkedResponse: public AsyncAbstractResponse {
    prtctd: AwsResponseFiller   _content                = {};
        size_t                  _filledLength           = {};
    public:                     AsyncChunkedResponse    (const String& contentType, AwsResponseFiller callback, AwsTemplateProcessor templateCallback=nullptr);
        inline  bool            _sourceValid            ()                                      const { return !!(_content); }
        virtual size_t          _fillBuffer             (uint8_t * buf, size_t maxLen) override;
    };
    class AsyncProgmemResponse: public AsyncAbstractResponse {
    prtctd:
        const uint8_t           * _content              = {};
        size_t                  _readLength             = {};
    public:                     AsyncProgmemResponse    (int code, const String& contentType, const uint8_t * content, size_t len, AwsTemplateProcessor callback=nullptr);
        inline  bool            _sourceValid            ()                                      const { return !!(_content); }
        virtual size_t          _fillBuffer             (uint8_t * buf, size_t maxLen) override;
    };
    class cbuf;
    class AsyncResponseStream: public AsyncAbstractResponse, public Print {
    prtctd:     cbuf            * _content              = {};
    public:                     ~AsyncResponseStream    ();
                                AsyncResponseStream     (const String& contentType, size_t bufferSize);
        using   Print           ::write;
        virtual size_t          _fillBuffer             (uint8_t *buf, size_t maxLen) override;
        size_t                  write                   (const uint8_t *data, size_t len);
        size_t                  write                   (uint8_t data);
        inline bool             _sourceValid            () const { return (_state < RESPONSE_END); }
    };
} // namespace

#endif /* ASYNCWEBSERVERRESPONSEIMPL_H_ */
