/*
	Parsing.cpp - HTTP request parsing.

	Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

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

#include <Arduino.h>
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include "detail/mimetable.h"

#define DEBUG_ESP_HTTP_SERVER
#ifdef DEBUG_ESP_PORT
#define DEBUG_OUTPUT DEBUG_ESP_PORT
#else
#define DEBUG_OUTPUT Serial
#endif

static const char Content_Type[] PROGMEM = "Content-Type";
static const char filename[] PROGMEM = "filename";


////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
static void *vealloc(void *ptr, size_t size) {
	void *newptr = realloc(ptr, size);
	if (!newptr) free(ptr);
	return newptr;
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
static char *readBytesWithTimeout(WiFiClient& client, size_t maxLength, size_t& dataLength, int timeout_ms) {
	char *buf = nullptr;
	dataLength = 0;
	while (dataLength < maxLength) {
		int tries = timeout_ms;
		size_t newLength;
		while (!(newLength = client.available()) && tries--) delay(1);

		if (!newLength) break;

		buf = (char*) vealloc(buf, dataLength + newLength + 1);
		if (!buf) return nullptr;

		client.readBytes(buf + dataLength, newLength);
		dataLength += newLength;
		buf[dataLength] = '\0';
	}
	return buf;
}




////////////////////////////////////////////////////////////////////////////////
// RESET ALL REQUEST VARIABLES TO DEFAULT FOR NEW INCOMING CONNECTION
// NOTE: WE DON'T TOUCH _requestBuffer BECAUSE OF A BUFFER REUSE OPTIMIZATION
////////////////////////////////////////////////////////////////////////////////
void ESP8266WebServer::resetRequest() {
	_method				= HTTP_ANY;
	_requestMethod		= nullptr;
	_requestPath		= nullptr;
	_requestParams		= nullptr;
	_requestVersion		= nullptr;
}




////////////////////////////////////////////////////////////////////////////////
// PARSE THE REQUEST FROM THE CLIENT BROWSER
////////////////////////////////////////////////////////////////////////////////
bool ESP8266WebServer::_parseRequest(WiFiClient& client) {
	//RESET HEADER VALUE
	for (int i=0; i<_headerKeysCount; i++) {
		_currentHeaders[i].value = "";
	}

	//RESET ALL REQUEST VARIABLES
	resetRequest();

	// READ THE FIRST LINE OF HTTP REQUEST
	//TODO: READ LITERALLY EVERYTHING
	//SINGLE BUFFER, PARSE IT OUT USING AWESOMENESS
	//String req = client.readStringUntil('\r');
	//client.readStringUntil('\n');
	_request = client.readString();

#	ifdef DEBUG_ESP_HTTP_SERVER
		Serial.println("REQUEST:");
		Serial.println(_request);
#	endif

	//REFERENCE TO BUFFER BUFFER
	_requestMethod =
	_requestBuffer =
		(char*) _request.c_str();


	//FIND REQUEST PATH AND VERSION
	char *buf = _requestBuffer;
	while (*buf) {
		if (*buf == ' ') {
			*buf++ = NULL;
			if (!_requestPath) {
				_requestPath	= buf;
			} else {
				_requestVersion	= buf;
				break;
			}

		} else if (*buf == '\r'  ||  *buf == '\n') {
			*buf++ = NULL;
			if (*buf == '\r'  ||  *buf == '\n') *buf++ = NULL;
			break;

		} else if (*buf == '?') {
			if (_requestPath  &&  !_requestParams  &&  !_requestVersion) {
				*buf++ = NULL;
				_requestParams	= buf;
			}
		}
		buf++;
	}

	//NOT WELL FORMATTED REQUEST HEADER
	if (!_requestVersion) return false;



	//TODO: PROCESS HEADERS HERE NOW



#	ifdef DEBUG_ESP_HTTP_SERVER
		DEBUG_OUTPUT.print("Method: --");
		DEBUG_OUTPUT.print(_requestMethod);
		DEBUG_OUTPUT.println("--");

		DEBUG_OUTPUT.print("Path: --");
		DEBUG_OUTPUT.print(_requestPath);
		DEBUG_OUTPUT.println("--");

		DEBUG_OUTPUT.print("Version: --");
		DEBUG_OUTPUT.print(_requestVersion);
		DEBUG_OUTPUT.println("--");
#	endif

	//PARSE REQUEST METHOD
	if (!strcmp(_requestMethod, "GET")) {
		_method = HTTP_GET;
	} else if (!strcmp(_requestMethod, "POST")) {
		_method = HTTP_POST;
	} else if (!strcmp(_requestMethod, "DELETE")) {
		_method = HTTP_DELETE;
	} else if (!strcmp(_requestMethod, "OPTIONS")) {
		_method = HTTP_OPTIONS;
	} else if (!strcmp(_requestMethod, "PUT")) {
		_method = HTTP_PUT;
	} else if (!strcmp(_requestMethod, "PATCH")) {
		_method = HTTP_PATCH;
	}


	//PARSE URL PARAMETERS
	if (_requestParams) {
		_parseArguments(_requestParams);
	}




	//attach handler
	for (_handler=_firstHandler; _handler; _handler=_handler->next()) {
		if (_handler->canHandle(_method, _currentUri)) {
			break;
		}
	}

	String formData;

	// below is needed only when POST type request
	switch (_method) {
		case HTTP_POST:
		case HTTP_PUT:
		case HTTP_PATCH:
		case HTTP_DELETE:


			String boundaryStr;
			String headerName;
			String headerValue;
			bool isForm = false;
			bool isEncoded = false;
			uint32_t contentLength = 0;
			//parse headers

			while (true) {
				req = client.readStringUntil('\r');
				client.readStringUntil('\n');

				if (req == "") break;//no moar headers
				int headerDiv = req.indexOf(':');
				if (headerDiv == -1) break;

				headerName = req.substring(0, headerDiv);
				headerValue = req.substring(headerDiv + 1);
				headerValue.trim();
				_collectHeader(headerName.c_str(),headerValue.c_str());

				#ifdef DEBUG_ESP_HTTP_SERVER
				DEBUG_OUTPUT.print("headerName: ");
				DEBUG_OUTPUT.println(headerName);
				DEBUG_OUTPUT.print("headerValue: ");
				DEBUG_OUTPUT.println(headerValue);
				#endif

				if (headerName.equalsIgnoreCase(FPSTR(Content_Type))) {
					using namespace mime;
					if (headerValue.startsWith(FPSTR(mimeTable[txt].mimeType))) {
						isForm = false;
					} else if (headerValue.startsWith(F("application/x-www-form-urlencoded"))) {
						isForm = false;
						isEncoded = true;
					} else if (headerValue.startsWith(F("multipart/"))) {
						boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
						boundaryStr.replace("\"","");
						isForm = true;
					}

				} else if (headerName.equalsIgnoreCase(F("Content-Length"))) {
					contentLength = headerValue.toInt();

				} else if (headerName.equalsIgnoreCase(F("Host"))) {
					_hostHeader = headerValue;
				}
			}

			if (!isForm) {
				size_t plainLength;
				char* plainBuf = readBytesWithTimeout(client, contentLength, plainLength, HTTP_MAX_POST_WAIT);

				if (plainLength < contentLength) {
					free(plainBuf);
					return false;
				}

				if (contentLength > 0) {
					if (isEncoded){
						_parseArguments(plainBuf, false);

					} else {
						//plain post json or other data
						RequestArgument& arg = _currentArgs[_currentArgCount++];
						arg.key = "plain";
						//arg.value = String(plainBuf);
						//TODO: THIS NEEDS FIXED!
					}

#					ifdef DEBUG_ESP_HTTP_SERVER
						DEBUG_OUTPUT.print("Plain: ");
						DEBUG_OUTPUT.println(plainBuf);
#					endif

					free(plainBuf);
				}
			}

			if (isForm) {
				if (!_parseForm(client, boundaryStr, contentLength)) {
					return false;
				}
			}
		break;


		default:
			String headerName;
			String headerValue;
			//parse headers
			while (true) {
				req = client.readStringUntil('\r');
				client.readStringUntil('\n');
				if (req == "") break;//no moar headers
				int headerDiv = req.indexOf(':');
				if (headerDiv == -1){
					break;
				}
				headerName = req.substring(0, headerDiv);
				headerValue = req.substring(headerDiv + 2);
				_collectHeader(headerName.c_str(),headerValue.c_str());

#				ifdef DEBUG_ESP_HTTP_SERVER
					DEBUG_OUTPUT.print("headerName: ");
					DEBUG_OUTPUT.println(headerName);
					DEBUG_OUTPUT.print("headerValue: ");
					DEBUG_OUTPUT.println(headerValue);
#				endif

				if (headerName.equalsIgnoreCase("Host")) {
					_hostHeader = headerValue;
				}
			break;
		}
		_parseArguments(_requestParams);
	}
	client.flush();

#	ifdef DEBUG_ESP_HTTP_SERVER
//		DEBUG_OUTPUT.print("Request: ");
//		DEBUG_OUTPUT.println(url);
		DEBUG_OUTPUT.print(" Arguments: ");
		DEBUG_OUTPUT.println(_requestParams);
#	endif

	return true;
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
bool ESP8266WebServer::_collectHeader(const char *headerName, const char *headerValue) {
	for (int i = 0; i < _headerKeysCount; i++) {
		if (!strcasecmp(_currentHeaders[i].key, headerName)) {
			_currentHeaders[i].value = headerValue;
			return true;
		}
	}
	return false;
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
void ESP8266WebServer::_parseArguments(char *data, bool reset) {

#ifdef DEBUG_ESP_HTTP_SERVER
	DEBUG_OUTPUT.print("args: ");
	DEBUG_OUTPUT.println(data);
#endif

	//RESET IF THIS IS A NEW WEB REQUEST
	//THIS IS TO ALLOW MULTIPLE ARGUMENT SETS PER REQUEST
	//RIGHT NOW THERE ARE BOTH URL PARAMETERS AND POST DATA
	if (reset) {
		if (_currentArgs) delete[] _currentArgs;
		_currentArgs		= nullptr;
		_currentArgCount	= 0;

		if (!data  ||  !*data) {
			_currentArgs = new RequestArgument[1];
			return;
		}

	//WE'RE NOT RESETTING, BUT WE ALSO HAVE NO NEW DATA TO PROCESS
	} else if (!data  ||  !*data) {
		return;
	}


	//WE HAVE A MINIMUM OF 1 ARGUMENT AUTOMATICALLY
	if (_currentArgCount < 1) {
		_currentArgCount = 1;
	}


	//FAST FIRST PASS - FIND THE TOTAL NUMBER OF ARGUMENTS
	char *ptr = data;
	while (*ptr) {
		if (*ptr == '&') _currentArgCount++;
		ptr++;
	}


#ifdef DEBUG_ESP_HTTP_SERVER
	DEBUG_OUTPUT.print("args count: ");
	DEBUG_OUTPUT.println(_currentArgCount);
#endif



	int		len				= ptr - data; //MUST COME BEFORE RESETTING PTR
	ptr						= data;
	char	*name			= ptr;
	char	*value			= nullptr;
	int		argid			= 0;  //TODO: THIS NEEDS TO BE CLASS LEVEL DUE TO MULTIPLE PARAMS GROUPS BEING PROCESSED
	_currentArgs			= new RequestArgument[_currentArgCount+1];

	while (len--) {
		if (*ptr == '&'  ||  *ptr == NULL) {
			bool brk		= (*ptr == NULL);
			*ptr			= NULL;

			RequestArgument &arg = _currentArgs[argid++];

			if (value) {
				arg.key		= urlDecode(name,	value - name);
				arg.value	= urlDecode(value,	ptr - value);
			} else {
				arg.key		= urlDecode(name,	ptr - name);
			}

			name			= ptr + 1;
			value			= nullptr;

			if (brk) break;

		} else if (*ptr == '='  &&  value == nullptr) {
			*ptr			= NULL;
			value			= ptr + 1;
		}

		ptr++;
	}
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
void ESP8266WebServer::_uploadWriteByte(uint8_t b) {
	if (_currentUpload->currentSize == HTTP_UPLOAD_BUFLEN) {
		if (_handler && _handler->canUpload(_currentUri)) {
			_handler->upload(*this, _currentUri, *_currentUpload);
		}

		_currentUpload->totalSize += _currentUpload->currentSize;
		_currentUpload->currentSize = 0;
	}

	_currentUpload->buf[_currentUpload->currentSize++] = b;
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
uint8_t ESP8266WebServer::_uploadReadByte(WiFiClient& client) {
	int res = client.read();
	if (res == -1) {
		while (!client.available() && client.connected()){
			yield();
		}
		res = client.read();
	}
	return (uint8_t)res;
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
bool ESP8266WebServer::_parseForm(WiFiClient& client, String boundary, uint32_t len) {
	(void) len;
#ifdef DEBUG_ESP_HTTP_SERVER
	DEBUG_OUTPUT.print("Parse Form: Boundary: ");
	DEBUG_OUTPUT.print(boundary);
	DEBUG_OUTPUT.print(" Length: ");
	DEBUG_OUTPUT.println(len);
#endif
	String line;
	int retry = 0;
	do {
		line = client.readStringUntil('\r');
		++retry;
	} while (line.length() == 0 && retry < 3);

	client.readStringUntil('\n');
	//start reading the form
	if (line == ("--"+boundary)){
		RequestArgument* postArgs = new RequestArgument[32];
		int postArgsLen = 0;
		while(1) {
			String argName;
			String argValue;
			String argType;
			String argFilename;
			bool argIsFile = false;

			line = client.readStringUntil('\r');
			client.readStringUntil('\n');
			if (line.length() > 19 && line.substring(0, 19).equalsIgnoreCase(F("Content-Disposition"))) {
				int nameStart = line.indexOf('=');
				if (nameStart != -1) {
					argName = line.substring(nameStart+2);
					nameStart = argName.indexOf('=');
					if (nameStart == -1) {
						argName = argName.substring(0, argName.length() - 1);
					} else {
						argFilename = argName.substring(nameStart+2, argName.length() - 1);
						argName = argName.substring(0, argName.indexOf('"'));
						argIsFile = true;
#ifdef DEBUG_ESP_HTTP_SERVER
						DEBUG_OUTPUT.print("PostArg FileName: ");
						DEBUG_OUTPUT.println(argFilename);
#endif
						//use GET to set the filename if uploading using blob
						if (!strcmp(argFilename.c_str(), "blob") && hasArg(filename)) {
							argFilename = arg(FPSTR(filename));
						}
					}
#ifdef DEBUG_ESP_HTTP_SERVER
					DEBUG_OUTPUT.print("PostArg Name: ");
					DEBUG_OUTPUT.println(argName);
#endif
					using namespace mime;
					argType = FPSTR(mimeTable[txt].mimeType);
					line = client.readStringUntil('\r');
					client.readStringUntil('\n');
					if (line.length() > 12 && line.substring(0, 12).equalsIgnoreCase(FPSTR(Content_Type))){
						argType = line.substring(line.indexOf(':')+2);
						//skip next line
						client.readStringUntil('\r');
						client.readStringUntil('\n');
					}
#ifdef DEBUG_ESP_HTTP_SERVER
					DEBUG_OUTPUT.print("PostArg Type: ");
					DEBUG_OUTPUT.println(argType);
#endif
					if (!argIsFile){
						while(1) {
							line = client.readStringUntil('\r');
							client.readStringUntil('\n');
							if (line.startsWith("--"+boundary)) break;
							if (argValue.length() > 0) argValue += "\n";
							argValue += line;
						}
#ifdef DEBUG_ESP_HTTP_SERVER
						DEBUG_OUTPUT.print("PostArg Value: ");
						DEBUG_OUTPUT.println(argValue);
						DEBUG_OUTPUT.println();
#endif

						RequestArgument& arg = postArgs[postArgsLen++];
						//arg.key = argName;
						//arg.value = argValue;
						//TODO fix these!

						if (line == ("--"+boundary+"--")){
#ifdef DEBUG_ESP_HTTP_SERVER
							DEBUG_OUTPUT.println("Done Parsing POST");
#endif
							break;
						}
					} else {
						_currentUpload.reset(new HTTPUpload());
						_currentUpload->status = UPLOAD_FILE_START;
						_currentUpload->name = argName;
						_currentUpload->filename = argFilename;
						_currentUpload->type = argType;
						_currentUpload->totalSize = 0;
						_currentUpload->currentSize = 0;
#ifdef DEBUG_ESP_HTTP_SERVER
						DEBUG_OUTPUT.print("Start File: ");
						DEBUG_OUTPUT.print(_currentUpload->filename);
						DEBUG_OUTPUT.print(" Type: ");
						DEBUG_OUTPUT.println(_currentUpload->type);
#endif
						if(_handler && _handler->canUpload(_currentUri))
							_handler->upload(*this, _currentUri, *_currentUpload);
						_currentUpload->status = UPLOAD_FILE_WRITE;
						uint8_t argByte = _uploadReadByte(client);
readfile:
						while(argByte != 0x0D){
							if (!client.connected()) return _parseFormUploadAborted();
							_uploadWriteByte(argByte);
							argByte = _uploadReadByte(client);
						}

						argByte = _uploadReadByte(client);
						if (!client.connected()) return _parseFormUploadAborted();
						if (argByte == 0x0A){
							argByte = _uploadReadByte(client);
							if (!client.connected()) return _parseFormUploadAborted();
							if ((char)argByte != '-'){
								//continue reading the file
								_uploadWriteByte(0x0D);
								_uploadWriteByte(0x0A);
								goto readfile;
							} else {
								argByte = _uploadReadByte(client);
								if (!client.connected()) return _parseFormUploadAborted();
								if ((char)argByte != '-'){
									//continue reading the file
									_uploadWriteByte(0x0D);
									_uploadWriteByte(0x0A);
									_uploadWriteByte((uint8_t)('-'));
									goto readfile;
								}
							}

							uint8_t endBuf[boundary.length()];
							client.readBytes(endBuf, boundary.length());

							if (strstr((const char*)endBuf, boundary.c_str()) != NULL){
								if(_handler && _handler->canUpload(_currentUri))
									_handler->upload(*this, _currentUri, *_currentUpload);
								_currentUpload->totalSize += _currentUpload->currentSize;
								_currentUpload->status = UPLOAD_FILE_END;
								if(_handler && _handler->canUpload(_currentUri))
									_handler->upload(*this, _currentUri, *_currentUpload);
#ifdef DEBUG_ESP_HTTP_SERVER
								DEBUG_OUTPUT.print("End File: ");
								DEBUG_OUTPUT.print(_currentUpload->filename);
								DEBUG_OUTPUT.print(" Type: ");
								DEBUG_OUTPUT.print(_currentUpload->type);
								DEBUG_OUTPUT.print(" Size: ");
								DEBUG_OUTPUT.println(_currentUpload->totalSize);
#endif
								line = client.readStringUntil(0x0D);
								client.readStringUntil(0x0A);
								if (line == "--"){
#ifdef DEBUG_ESP_HTTP_SERVER
									DEBUG_OUTPUT.println("Done Parsing POST");
#endif
									break;
								}
								continue;
							} else {
								_uploadWriteByte(0x0D);
								_uploadWriteByte(0x0A);
								_uploadWriteByte((uint8_t)('-'));
								_uploadWriteByte((uint8_t)('-'));
								uint32_t i = 0;
								while(i < boundary.length()){
									_uploadWriteByte(endBuf[i++]);
								}
								argByte = _uploadReadByte(client);
								goto readfile;
							}
						} else {
							_uploadWriteByte(0x0D);
							goto readfile;
						}
						break;
					}
				}
			}
		}

		int iarg;
		int totalArgs = ((32 - postArgsLen) < _currentArgCount)?(32 - postArgsLen):_currentArgCount;
		for (iarg = 0; iarg < totalArgs; iarg++){
			RequestArgument& arg = postArgs[postArgsLen++];
			arg.key = _currentArgs[iarg].key;
			arg.value = _currentArgs[iarg].value;
		}
		if (_currentArgs) delete[] _currentArgs;
		_currentArgs = new RequestArgument[postArgsLen];
		for (iarg = 0; iarg < postArgsLen; iarg++){
			RequestArgument& arg = _currentArgs[iarg];
			arg.key = postArgs[iarg].key;
			arg.value = postArgs[iarg].value;
		}
		_currentArgCount = iarg;
		if (postArgs)
			delete[] postArgs;
		return true;
	}
#ifdef DEBUG_ESP_HTTP_SERVER
	DEBUG_OUTPUT.print("Error: line: ");
	DEBUG_OUTPUT.println(line);
#endif
	return false;
}




////////////////////////////////////////////////////////////////////////////////
// DECODE URL ENCODING - INLINE REPLACE WITHIN THE STRING
////////////////////////////////////////////////////////////////////////////////
char *ESP8266WebServer::urlDecode(char *text, int len) {
	if (!text) return text;

	char *src	= text;
	char *dst	= text;
	char temp[]	= "00";

	while (*src) {
		if (len-- < 1) break;

		if (*src == '%') {
			if (!src[1]  ||  !src[2]) break;

			temp[0] = src[1];
			temp[1] = src[2];

			len -= 2;
			src += 2;
			*dst = (char) strtol(temp, nullptr, 16);

		} else if (*src == '+') {
			*dst = ' ';

		} else {
			*dst = *src;
		}

		src++;
		dst++;
	}

	*dst = NULL;

	return text;
}




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
bool ESP8266WebServer::_parseFormUploadAborted(){
	_currentUpload->status = UPLOAD_FILE_ABORTED;

	if(_handler && _handler->canUpload(_currentUri)) {
		_handler->upload(*this, _currentUri, *_currentUpload);
	}

	return false;
}
