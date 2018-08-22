/*
	ESP8266WebServer.h - Dead simple web-server.
	Supports only one simultaneous client, knows how to handle GET and POST.

	Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.

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
	Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/


#ifndef __ESP8266_WEB_SERVER_H__
#define __ESP8266_WEB_SERVER_H__



#include <functional>
#include <memory>
#include <ESP8266WiFi.h>

#include "ESP8266WebServerHelper.h"



class ESP8266WebServer {
private:
	void _init();

public:
	ESP8266WebServer(IPAddress addr, int port=80);
	ESP8266WebServer(int port=80);
	virtual ~ESP8266WebServer();

	virtual void begin();
	virtual void begin(uint16_t port);
	virtual void handleClient();

	virtual void close();
	void stop();

	bool authenticate(const char *username, const char *password);

	void requestAuthentication(	HTTPAuthMethod mode=BASIC_AUTH,
								const char* realm = NULL,
								const String& authFailMsg = String("") );

	typedef std::function<void(void)> THandlerFunction;
	void on(const String &uri, THandlerFunction handler);
	void on(const String &uri, HTTPMethod method, THandlerFunction fn);
	void on(const String &uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn);
	void addHandler(RequestHandler* handler);
	void serveStatic(const char* uri, fs::FS& fs, const char* path, const char* cache_header = NULL );
	void onNotFound(THandlerFunction fn);  //called when handler is not assigned
	void onFileUpload(THandlerFunction fn); //handle file uploads

	const char *uri() const				{ return _currentUri; }
	HTTPMethod method() const			{ return _method; }
	virtual WiFiClient client() const	{ return _currentClient; }
	HTTPUpload& upload() const			{ return *_currentUpload; }




	////////////////////////////////////////////////////////////////////////////
	// GET AND POST ARGUMENTS (MERGED TOGETHER)
	////////////////////////////////////////////////////////////////////////////

	// GET ARGUMENTS COUNT
	int args() const {
		return _currentArgCount;
	}


	// GET REQUEST ARGUMENT VALUE BY NAME
	const char *arg(const char *name) const;

	inline String arg(String name) const {
		return this->arg(name.c_str());
	}



	// GET REQUEST ARGUMENT VALUE BY NUMBER
	const char *arg(int i) const;



	// GET REQUEST ARGUMENT NAME BY NUMBER
	const char *argName(int i) const;



	// CHECK IF ARGUMENT EXISTS
	bool hasArg(const char *name) const;

	inline bool hasArg(String name) const {
		return this->hasArg(name.c_str());
	}




	////////////////////////////////////////////////////////////////////////////
	// HEADERS
	////////////////////////////////////////////////////////////////////////////

	// get header count
	int headers() const {
		return _headerKeysCount;
	}



	// SET THE REQUEST HEADERS TO COLLECT
	void collectHeaders(const char* headerKeys[], const size_t headerKeysCount);



	// GET REQUEST HEADER VALUE BY NAME
	const char *header(const char *name) const;

	String header(String name) const {
		return String((const char*)this->header(name.c_str()));
	}



	// GET REQUEST HEADER VALUE BY NUMBER
	const char *header(int i) const;



	// GET REQUEST HEADER NAME BY NUMBER
	const char *headerName(int i) const;



	// CHECK IF HEADER EXISTS
	bool hasHeader(const char *name) const;

	bool hasHeader(String name) const {
		return this->hasHeader(name.c_str());
	}



	// GET REQUEST HOST HEADER IF AVAILABLE OR EMPTY STRING IF NOT
	String hostHeader() const {
		return _hostHeader;
	}




	////////////////////////////////////////////////////////////////////////////
	// RESPONSE DATA SENT BACK TO CLIENT
	//
	// INT CODE = 200 OK
	// INT CODE = 404 ERROR NOT FOUND
	// FULL LIST: https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
	//
	// CONTENT TYPE = "text/plain"
	// CONTENT TYPE = "text/html"
	////////////////////////////////////////////////////////////////////////////

	void send(int code, const char* content_type = NULL, const String& content = String(""));
	void send(int code, char* content_type, const String& content);
	void send(int code, const String& content_type, const String& content);
	void send_P(int code, PGM_P content_type, PGM_P content);
	void send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength);

	void setContentLength(const size_t contentLength);
	void sendHeader(const String& name, const String& value, bool first = false);
	void sendContent(const String& content);
	void sendContent_P(PGM_P content);
	void sendContent_P(PGM_P content, size_t size);


	template<typename T>
	size_t streamFile(T &file, const String& contentType) {
		_streamFileCore(file.size(), file.name(), contentType);
		return _currentClient.write(file);
	}




	////////////////////////////////////////////////////////////////////////////
	// FAST MEMORY INPLACE URL ENCODING DECODER
	////////////////////////////////////////////////////////////////////////////
	static char *urlDecode(char *text, int len=0x7fffffff);



protected:
	virtual size_t _currentClientWrite(const char* b, size_t l) { return _currentClient.write( b, l ); }
	virtual size_t _currentClientWrite_P(PGM_P b, size_t l) { return _currentClient.write_P( b, l ); }
	void _addRequestHandler(RequestHandler* handler);
	void _handleRequest();
	void _finalizeResponse();
	bool _parseRequest(WiFiClient& client);
	void _parseArguments(char *data, bool reset=true);
	static String _responseCodeToString(int code);
	bool _parseForm(WiFiClient& client, String boundary, uint32_t len);
	bool _parseFormUploadAborted();
	void _uploadWriteByte(uint8_t b);
	uint8_t _uploadReadByte(WiFiClient& client);
	void _prepareHeader(String& response, int code, const char* content_type, size_t contentLength);
	bool _collectHeader(const char* headerName, const char* headerValue);

	void _streamFileCore(const size_t fileSize, const String & fileName, const String & contentType);

	String _getRandomHexString();
	// for extracting Auth parameters
	String _extractParam(String& authReq, const String& param, const char delimit='"');


	// BUFFER IS THE ALLOCATED BUFFER
	// METHOD, PATH, AND VERSIONS ARE ALL POINTERS WITHIN THE SINGLE ALLOCATED BUFFER
	char *_requestBuffer;
	char *_requestMethod;
	char *_requestPath;
	char *_requestParams;
	char *_requestVersion;
	char *_requestHeaders;

	void resetRequest();


	WiFiServer  _server;

	WiFiClient  _currentClient;
	HTTPMethod  _method;
	const char *_currentUri;
	uint8_t     _currentVersion;
	HTTPClientStatus _currentStatus;
	unsigned long _statusChange;

	RequestHandler*  _handler;
	RequestHandler*  _firstHandler;
	RequestHandler*  _lastHandler;
	THandlerFunction _notFoundHandler;
	THandlerFunction _fileUploadHandler;

	int              _currentArgCount;
	RequestArgument* _currentArgs;
	std::unique_ptr<HTTPUpload> _currentUpload;

	int              _headerKeysCount;
	RequestArgument* _currentHeaders;
	size_t           _contentLength;
	String           _responseHeaders;

	String           _hostHeader;
	bool             _chunked;

	String           _snonce;  // Store noance and opaque for future comparison
	String           _sopaque;
	String           _srealm;  // Store the Auth realm between Calls



private:
	String			_request;
};


#endif //__ESP8266_WEB_SERVER_H__
