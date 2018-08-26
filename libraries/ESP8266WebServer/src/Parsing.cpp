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


baseline:
Sketch uses 411200 bytes (39%) of program storage space. Maximum is 1044464 bytes.
Global variables use 38732 bytes (47%) of dynamic memory, leaving 43188 bytes for local variables. Maximum is 81920 bytes.

early stage (no posting, should have fast buffer reading)
Sketch uses 406924 bytes (38%) of program storage space. Maximum is 1044464 bytes.
Global variables use 38824 bytes (47%) of dynamic memory, leaving 43096 bytes for local variables. Maximum is 81920 bytes.

current stage (everything is beautiful)
Sketch uses 407264 bytes (38%) of program storage space. Maximum is 1044464 bytes.
Global variables use 38812 bytes (47%) of dynamic memory, leaving 43108 bytes for local variables. Maximum is 81920 bytes.
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
// ATTEMPT TO REALLOCATE MEMORY - OR RETURN NULLPTR ON FAILURE
// UNLIKE NORMAL REALLOC, THIS WILL FREE() THE PREVIOUS BUFFER ON FAILURE
////////////////////////////////////////////////////////////////////////////////
template <class T>
static inline T *vealloc(T *ptr, size_t size) {
	T *newptr = (T*) realloc(ptr, size);
	if (!newptr) free(ptr);
	return newptr;
}




////////////////////////////////////////////////////////////////////////////////
// RESET ALL REQUEST VARIABLES TO DEFAULT FOR NEW INCOMING CONNECTION
////////////////////////////////////////////////////////////////////////////////
void ESP8266WebServer::resetRequest() {
	free(_request);
	_request			= nullptr;
	_requestPath		= nullptr;
	_requestVersion		= nullptr;
	_method				= HTTP_ANY;
	_statusCode			= 0;
	_headers.reset();
	_params.reset();
}




////////////////////////////////////////////////////////////////////////////////
// PARSE THE REQUEST FROM THE CLIENT BROWSER
////////////////////////////////////////////////////////////////////////////////
int ESP8266WebServer::_parseRequest(WiFiClient& client) {
	//RESET ALL REQUEST VARIABLES
	resetRequest();


	auto timeout	= millis();
	auto read		= 0;

	do {
		//TODO: MAKE READ TIMEOUT CONFIGURABLE
		if (millis() - timeout > 5000) return 408;

		//CHECK FOR AVAILABLE BYTES
		auto available	= client.available();
		if (!available) { yield(); continue; }

		//INCREASE BUFFER SIZE
		_request = vealloc(_request, read + available + 1);

		//NO ROOM TO EXPAND BUFFER? OUTPUT ERROR!
		if (!_request) return 413;

		//READ AVAILABLE BYTES INTO OUR BUFFER
		client.read(((unsigned char*)_request) + read, available);

		//INCREASE VALUE LETTING US KNOW HOW MUCH WE READ
		read += available;

		//NULL TERMINATE THE BUFFER STRING
		_request[read] = NULL;

	} while (!strstr(_request, "\r\n\r\n"));



	//TODO:	switch _request over to normal character buffer array
	//		load data until we find "\r\n\r\n"
	//		parse headers
	//		check if content-length is available
	//		if so, run a realloc
	//		compute old vs new buffer offsets
	//		adjust all arrays based on buffer offset different

	/*
		error checking:
		1) if we've read too much data (10kb?) without hitting \r\n\r\n, bail
		2) if alloc or realloc fails, bail
		3) if content length is too long, bail before even reading POST data
	*/



#	ifdef DEBUG_ESP_HTTP_SERVER
		DEBUG_OUTPUT.println(F("REQUEST:"));
		DEBUG_OUTPUT.println(_request);
#	endif



	//LOCAL VARIABLES
	char *_requestParams = nullptr;


	//PROCESS HEADERS
	char	*offset	= _request;
	char	*buf	= _headers.process(_request);
	int		hlength	= buf - _request;
	int		clength	= _headers.integer("Content-Length");


	while (read - hlength < clength) {
		//TODO: MAKE READ TIMEOUT CONFIGURABLE
		if (millis() - timeout > 5000) return 408;

		//CHECK FOR AVAILABLE BYTES
		auto available	= client.available();
		if (!available) { yield(); continue; }

		//INCREASE BUFFER SIZE
		_request = vealloc(_request, read + available + 1);

		//NO ROOM TO EXPAND BUFFER? OUTPUT ERROR!
		if (!_request) return 413;

		//READ AVAILABLE BYTES INTO OUR BUFFER
		client.read(((unsigned char*)_request) + read, available);

		//INCREASE VALUE LETTING US KNOW HOW MUCH WE READ
		read += available;

		//NULL TERMINATE THE BUFFER STRING
		_request[read] = NULL;
	}

	_headers.__offset(_request - offset);


	char *line = (char*) _headers.key(0);
	if (!line  ||  !*line) return 418;

	do {
		if (*line == ' ') {
			*line++				= NULL;

			if (!_requestPath) {
				_requestPath	= line;

			} else if (!_requestVersion) {
				_requestVersion	= line;

			} else {
				return 400;
			}

		} else if (*line == '?') {
			if (_requestPath  &&  !_requestParams  &&  !_requestVersion) {
				*line++			= NULL;
				_requestParams	= line;
			}

		} else {
			line++;
		}
	} while (*line);

	if (!_requestVersion) return 400;



#	ifdef DEBUG_ESP_HTTP_SERVER
		DEBUG_OUTPUT.print(F("Method: --"));
		DEBUG_OUTPUT.print(_headers.key(0));
		DEBUG_OUTPUT.println(F("--"));

		DEBUG_OUTPUT.print(F("Path: --"));
		DEBUG_OUTPUT.print(_requestPath);
		DEBUG_OUTPUT.println(F("--"));

		DEBUG_OUTPUT.print(F("Version: --"));
		DEBUG_OUTPUT.print(_requestVersion);
		DEBUG_OUTPUT.println(F("--"));
#	endif


	//PARSE REQUEST METHOD
	const char *method = _headers.key(0);
	if (!strcasecmp(method, "GET")) {
		_method = HTTP_GET;
	} else if (!strcasecmp(method, "POST")) {
		_method = HTTP_POST;
	} else if (!strcasecmp(method, "DELETE")) {
		_method = HTTP_DELETE;
	} else if (!strcasecmp(method, "OPTIONS")) {
		_method = HTTP_OPTIONS;
	} else if (!strcasecmp(method, "PUT")) {
		_method = HTTP_PUT;
	} else if (!strcasecmp(method, "PATCH")) {
		_method = HTTP_PATCH;
	}


#	ifdef DEBUG_ESP_HTTP_SERVER
		DEBUG_OUTPUT.print(F("Method detected: "));
		DEBUG_OUTPUT.println(_method);
#	endif


	//PARSE URL PARAMETERS
	if (_requestParams) {
#		ifdef DEBUG_ESP_HTTP_SERVER
			DEBUG_OUTPUT.print(F("Params: --"));
			DEBUG_OUTPUT.print(_requestParams);
			DEBUG_OUTPUT.println(F("--"));
#		endif
		_params.process(_requestParams, true);
	}


	switch (_method) {
		case HTTP_POST:
		case HTTP_PUT:
		case HTTP_PATCH:
		case HTTP_DELETE:
			if (*buf) {
				//TODO: THIS IS BASED ON ENCODING METHOD, CHECK THAT FIRST
				_params.process(buf, false);
			}
		break;
	}


	//attach handler
	for (_handler=_firstHandler; _handler; _handler=_handler->next()) {
		if (_handler->canHandle(_method, _requestPath)) {
			break;
		}
	}


	client.flush();

	return 200;
}


//	String formData;

	// below is needed only when POST type request
/*
	switch (_method) {
		case HTTP_POST:
		case HTTP_PUT:
		case HTTP_PATCH:
		case HTTP_DELETE:

			{
				String boundaryStr;
				String headerName;
				String headerValue;
				bool isForm = false;
				bool isEncoded = false;
				uint32_t contentLength = 0;
				//parse headers

				//while (true) {
					//parse the request headers from POST data
					//NOTE: THIS PROCESS HAS BEEN MOVED ABOVE
				//}
					/*
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
					DEBUG_OUTPUT.print(F("headerName: "));
					DEBUG_OUTPUT.println(headerName);
					DEBUG_OUTPUT.print(F("headerValue: "));
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

				}*/
/*
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
							DEBUG_OUTPUT.print(F("Plain: "));
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
			}
		break;


		default:
			{
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

#					ifdef DEBUG_ESP_HTTP_SERVER
						DEBUG_OUTPUT.print(F("headerName: "));
						DEBUG_OUTPUT.println(headerName);
						DEBUG_OUTPUT.print(F("headerValue: "));
						DEBUG_OUTPUT.println(headerValue);
#					endif

					if (headerName.equalsIgnoreCase("Host")) {
						_hostHeader = headerValue;
					}
				}
			}
		break;


		_parseArguments(_requestParams);
	}*/




////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
/*
void ESP8266WebServer::_uploadWriteByte(uint8_t b) {
	if (_currentUpload->currentSize == HTTP_UPLOAD_BUFLEN) {
		if (_handler && _handler->canUpload(_requestPath)) {
			_handler->upload(*this, _requestPath, *_currentUpload);
		}

		_currentUpload->totalSize += _currentUpload->currentSize;
		_currentUpload->currentSize = 0;
	}

	_currentUpload->buf[_currentUpload->currentSize++] = b;
}
*/



////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
/*
uint8_t ESP8266WebServer::_uploadReadByte(WiFiClient& client) {
	int res = client.read();
	if (res == -1) {
		while (!client.available() && client.connected()) {
			yield();
		}
		res = client.read();
	}
	return (uint8_t)res;
}
*/



////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
/*
bool ESP8266WebServer::_parseForm(WiFiClient& client, String boundary, uint32_t len) {
	return false;
}
*/
	/*
	(void) len;
#ifdef DEBUG_ESP_HTTP_SERVER
	DEBUG_OUTPUT.print(F("Parse Form: Boundary: "));
	DEBUG_OUTPUT.print(boundary);
	DEBUG_OUTPUT.print(F(" Length: "));
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
						DEBUG_OUTPUT.print(F("PostArg FileName: "));
						DEBUG_OUTPUT.println(argFilename);
#endif
						//use GET to set the filename if uploading using blob
						if (!strcmp(argFilename.c_str(), "blob") && hasArg(filename)) {
							argFilename = arg(FPSTR(filename));
						}
					}
#ifdef DEBUG_ESP_HTTP_SERVER
					DEBUG_OUTPUT.print(F("PostArg Name: "));
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
					DEBUG_OUTPUT.print(F("PostArg Type: "));
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
						DEBUG_OUTPUT.print(F("PostArg Value: "));
						DEBUG_OUTPUT.println(argValue);
						DEBUG_OUTPUT.println();
#endif

						RequestArgument& arg = postArgs[postArgsLen++];
						//arg.key = argName;
						//arg.value = argValue;
						//TODO fix these!

						if (line == ("--"+boundary+"--")){
#ifdef DEBUG_ESP_HTTP_SERVER
							DEBUG_OUTPUT.println(F("Done Parsing POST"));
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
						DEBUG_OUTPUT.print(F("Start File: "));
						DEBUG_OUTPUT.print(_currentUpload->filename);
						DEBUG_OUTPUT.print(F(" Type: "));
						DEBUG_OUTPUT.println(_currentUpload->type);
#endif
						if(_handler && _handler->canUpload(_requestPath))
							_handler->upload(*this, _requestPath, *_currentUpload);
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
								if(_handler && _handler->canUpload(_requestPath))
									_handler->upload(*this, _requestPath, *_currentUpload);
								_currentUpload->totalSize += _currentUpload->currentSize;
								_currentUpload->status = UPLOAD_FILE_END;
								if(_handler && _handler->canUpload(_requestPath))
									_handler->upload(*this, _requestPath, *_currentUpload);
#ifdef DEBUG_ESP_HTTP_SERVER
								DEBUG_OUTPUT.print(F("End File: "));
								DEBUG_OUTPUT.print(_currentUpload->filename);
								DEBUG_OUTPUT.print(F(" Type: "));
								DEBUG_OUTPUT.print(_currentUpload->type);
								DEBUG_OUTPUT.print(F(" Size: "));
								DEBUG_OUTPUT.println(_currentUpload->totalSize);
#endif
								line = client.readStringUntil(0x0D);
								client.readStringUntil(0x0A);
								if (line == "--"){
#ifdef DEBUG_ESP_HTTP_SERVER
									DEBUG_OUTPUT.println(F("Done Parsing POST"));
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
	DEBUG_OUTPUT.print(F("Error: line: "));
	DEBUG_OUTPUT.println(line);
#endif
*/





////////////////////////////////////////////////////////////////////////////////
// ??
////////////////////////////////////////////////////////////////////////////////
/*
bool ESP8266WebServer::_parseFormUploadAborted(){
	_currentUpload->status = UPLOAD_FILE_ABORTED;

	if(_handler && _handler->canUpload(_requestPath)) {
		_handler->upload(*this, _requestPath, *_currentUpload);
	}

	return false;
}
*/
