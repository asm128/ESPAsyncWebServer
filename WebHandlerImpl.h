#include "llc_array_pod.h"

#include <string>
#include <time.h>
#include "stddef.h"

#ifdef ASYNCWEBSERVER_REGEX
#   include <regex>
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
#ifndef ASYNCWEBSERVERHANDLERIMPL_H_
#define ASYNCWEBSERVERHANDLERIMPL_H_

namespace llc
{
    class SAWHStatic : public AsyncWebHandler {
    prtctd:
        using                   File                    = fs::File;
        using                   FS                      = fs::FS;
        FS                      _fs;
        String                  _uri                    = {};
        String                  _path                   = {};
        String                  _default_file           = {};
        String                  _cache_control          = {};
        String                  _last_modified          = {};
        AwsTemplateProcessor    _callback               = {};
        bool                    _isDir                  = {};
        bool                    _gzipFirst              = {};
        uint8_t                 _gzipStats              = {};
        bool                    _getFile                (SAWServerRequest * request);
        bool                    _fileExists             (SAWServerRequest * request, const String & path);
        uint8_t                 _countBits              (const uint8_t value) const;
    public:                     SAWHStatic              (const char * uri, FS & fs, const char * path, const char * cache_control);
        inline SAWHStatic&      setTemplateProcessor    (AwsTemplateProcessor newCallback) { _callback = newCallback; return *this; }
        virtual bool            canHandle               (SAWServerRequest * request) override final;
        virtual void            handleRequest           (SAWServerRequest * request) override final;
        SAWHStatic&             setIsDir                (bool isDir);
        SAWHStatic&             setDefaultFile          (const char * filename);
        SAWHStatic&             setCacheControl         (const char * cache_control);
        SAWHStatic&             setLastModified         (const char * last_modified);
        SAWHStatic&             setLastModified         (struct tm * last_modified);
#ifdef LLC_ESP8266
        SAWHStatic&             setLastModified         (time_t last_modified);
        SAWHStatic&             setLastModified         (); //sets to current time. Make sure sntp is runing and time is updated
#endif
    };

    class SAWHCallback : public AsyncWebHandler {
    prtctd: String                  _uri                    = {};
        WebRequestMethodComposite   _method                 = HTTP_ANY;
        ArRequestHandlerFunction    _onRequest              = {};
        ArUploadHandlerFunction     _onUpload               = {};
        ArBodyHandlerFunction       _onBody                 = {};
        bool                        _isRegex                = {};
    public: inline  void            setUri                  (const String & uri)                          { _uri = uri; _isRegex = uri.startsWith("^") && uri.endsWith("$"); }
        inline  void                setMethod               (WebRequestMethodComposite method)            { _method = method; }
        inline  void                onRequest               (const ArRequestHandlerFunction & fn)         { _onRequest  = fn; }
        inline  void                onUpload                (const ArUploadHandlerFunction  & fn)         { _onUpload   = fn; }
        inline  void                onBody                  (const ArBodyHandlerFunction    & fn)         { _onBody     = fn; }
        virtual bool                canHandle               (SAWServerRequest * request)  override final  {
            if(!_onRequest)
                return false;
            if(0 == (_method & request->method()))
                return false;
    #ifdef ASYNCWEBSERVER_REGEX
            if (_isRegex) {
                std::regex pattern(_uri.c_str());
                std::smatch matches;
                std::string s(request->url().c_str());
                if(false == std::regex_search(s, matches, pattern))
                    return false;
                for (size_t i = 1; i < matches.size(); ++i) // start from 1
                    request->_addPathParam(matches[i].str().c_str());
            } else
    #endif
            if(_uri.length()) {
                if((_uri != request->url() && false == request->url().startsWith(_uri + "/")))
                    return false;
                else if(_uri.startsWith("/*.")) {
                    String uriTemplate = String (_uri);
                    uriTemplate = uriTemplate.substring(uriTemplate.lastIndexOf("."));
                    if (false == request->url().endsWith(uriTemplate))
                        return false;
                }
                else if(_uri.endsWith("*")) {
                    String uriTemplate = _uri.substring(0, uriTemplate.length() - 1);
                    if(false == request->url().startsWith(uriTemplate))
                        return false;
                }
            }
            request->addInterestingHeader("ANY");
            return true;
        }
        virtual bool isRequestHandlerTrivial      ()  override final   { return false == _onRequest; }
        virtual void handleBody                   (SAWServerRequest * request, uint8_t * data, size_t len, size_t index, size_t total) override final {
            if((_username.length() && _password.length()) && false == request->authenticate(_username.c_str(), _password.c_str()))
                request->requestAuthentication();
            if(_onBody)
                _onBody(request, data, len, index, total);
            //else
            //    request->send(500);
        }
        virtual void handleUpload(SAWServerRequest * request, const String & filename, size_t index, uint8_t * data, size_t len, bool final) override final {
            if((_username.length() && _password.length()) && false == request->authenticate(_username.c_str(), _password.c_str()))
                request->requestAuthentication();
            if(_onUpload)
                _onUpload(request, filename, index, data, len, final);
            //else
            //    request->send(500);
        }
        virtual void handleRequest(SAWServerRequest * request) override final {
            if((_username.length() && _password.length()) && false == request->authenticate(_username.c_str(), _password.c_str()))
                request->requestAuthentication();
            if(_onRequest)
                _onRequest(request);
            else
                request->send(500);
        }
    };
} // namespace

#endif /* ASYNCWEBSERVERHANDLERIMPL_H_ */
