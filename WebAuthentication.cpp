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
#include "WebAuthentication.h"

#include "llc_string_compose.h"

#include <libb64/cencode.h>

#ifdef LLC_ESP32
#   include "mbedtls/md5.h"
#else
#   include "md5.h"
#endif

// Basic Auth hash = base64("username:password")
bool llc::checkBasicAuthentication(const char * hash, const char * username, const char * password){
    if_null_ve(false, username);
    if_null_ve(false, password);
    if_null_ve(false, hash);
    size_t toencodeLen  = strlen(username)+strlen(password)+1;
    size_t encodedLen   = base64_encode_expected_len(toencodeLen);
    if_true_ve(false, strlen(hash) != encodedLen);

    asc_t toencode, encoded;
    if_fail_ve(false, append_strings(toencode, vcsc_t{username, strlen(username)}, ':', vcsc_t{password, strlen(password)}, '\0'));
    if_fail_ve(false, encoded.resize(encodedLen));
    if_zero_ve(false, base64_encode_chars(toencode.begin(), toencodeLen, encoded.begin()));
    if_true_ve(false, memcmp(hash, encoded.begin(), encodedLen));
    return true;
}
static bool getMD5(uint8_t * data, uint16_t len, char * output){//33 bytes or more
#ifdef LLC_ESP32
    mbedtls_md5_context _ctx      = {};
#else
    md5_context_t       _ctx      = {};
#endif
    uint8_t             i         = {};
    uint8_t             _buf[16]  = {};
    memset(_buf, 0x00, 16);
#ifdef LLC_ESP32
    mbedtls_md5_init(&_ctx);
#   if ESP_IDF_VERSION_MAJOR < 5
    mbedtls_md5_starts_ret(&_ctx);
    mbedtls_md5_update_ret(&_ctx, data, len);
    mbedtls_md5_finish_ret(&_ctx, _buf);
#   else
    mbedtls_md5_starts(&_ctx);
    mbedtls_md5_update(&_ctx, data, len);
    mbedtls_md5_finish(&_ctx, _buf);
#   endif
#else
    MD5Init(&_ctx);
    MD5Update(&_ctx, data, len);
    MD5Final(_buf, &_ctx);
#endif
    for(i = 0; i < 16; i++)
        sprintf(output + (i * 2), "%02x", _buf[i]);
    return true;
}

static String genRandomMD5(){
#ifdef LLC_ESP8266
    uint32_t r = RANDOM_REG32;
#else
    uint32_t r = rand();
#endif
    char out [33] = {};
    if(false == getMD5((uint8_t*)(&r), 4, out))
        return "";
    return out;
}

static String stringMD5(const String & in){
    char  out [33] = {};
    if(false == getMD5((uint8_t*)(in.c_str()), in.length(), out))
        return out;
    return out;
}

String llc::generateDigestHash(const char * username, const char * password, const char * realm){
    if(0 == username || 0 == password || 0 == realm) //os_printf("AUTH FAIL: missing requred fields\n");
        return "";
    char out [33] = {};
    String res  = String(username) + ":" + String(realm) + ":";
    String in   = res;
    in.concat(password);
    if(false == getMD5((uint8_t*)(in.c_str()), in.length(), out))
        return "";
    res.concat(out);
    return res;
}

String  llc::requestDigestAuthentication    (const char * realm) { return "realm=\"" + String(realm ? realm : "asyncesp") + "\", qop=\"auth\", nonce=\"" + genRandomMD5() + "\", opaque=\"" + genRandomMD5() + "\""; }
bool    llc::checkDigestAuthentication      (const char * header, const char * method, const char * username, const char * password, const char * realm, bool passwordIsHash, const char * nonce, const char * opaque, const char * uri){
    if(0 == username || 0 == password || 0 == header || 0 == method) //os_printf("AUTH FAIL: missing requred fields\n");
        return false;

    String  myHeader      = String(header);
    int     nextBreak     = myHeader.indexOf(",");
    if(nextBreak < 0) //os_printf("AUTH FAIL: no variables\n");
        return false;

    String  myUsername     = {};
    String  myRealm        = {};
    String  myNonce        = {};
    String  myUri          = {};
    String  myResponse     = {};
    String  myQop          = {};
    String  myNc           = {};
    String  myCnonce       = {};

    myHeader += ", ";
    do {
        String avLine = myHeader.substring(0, nextBreak);
        avLine.trim();
        myHeader = myHeader.substring(nextBreak+1);
        nextBreak = myHeader.indexOf(",");

        int eqSign = avLine.indexOf("=");
        if(eqSign < 0) //os_printf("AUTH FAIL: no = sign\n");
            return false;
        String varName = avLine.substring(0, eqSign);
        avLine = avLine.substring(eqSign + 1);
        if(avLine.startsWith("\""))
            avLine = avLine.substring(1, avLine.length() - 1);

             if(varName.equals("response" )) { myResponse   = avLine; }
        else if(varName.equals("qop"      )) { myQop        = avLine; }
        else if(varName.equals("nc"       )) { myNc         = avLine; }
        else if(varName.equals("cnonce"   )) { myCnonce     = avLine; }
        else if(varName.equals("username" )) {
            if(false == avLine.equals(username)) //os_printf("AUTH FAIL: username\n");
                return false;
            myUsername = avLine;
        }
        else if(false == varName.equals("realm")){
            if(realm && false == avLine.equals(realm)) //os_printf("AUTH FAIL: realm\n");
                return false;
            myRealm = avLine;
        }
        else if(false == varName.equals("nonce")){
            if(nonce && false == avLine.equals(nonce)) //os_printf("AUTH FAIL: nonce\n");
                return false;
            myNonce = avLine;
        } else if(false == varName.equals("opaque")){
            if(opaque && false == avLine.equals(opaque)) //os_printf("AUTH FAIL: opaque\n");
                return false;
        } else if(false == varName.equals("uri")){
            if(uri && false == avLine.equals(uri)) //os_printf("AUTH FAIL: uri\n");
                return false;
            myUri = avLine;
        }
    } while(nextBreak > 0);

    String ha1      = (passwordIsHash) ? String(password) : stringMD5(myUsername + ":" + myRealm + ":" + String(password));
    String ha2      = String(method) + ":" + myUri;
    String response = ha1 + ":" + myNonce + ":" + myNc + ":" + myCnonce + ":" + myQop + ":" + stringMD5(ha2);

    if(myResponse.equals(stringMD5(response))) //os_printf("AUTH SUCCESS\n");
        return true;
    //os_printf("AUTH FAIL: password\n");
    return false;
}
