

#include "ESP8266WebServerHeader.h"



////////////////////////////////////////////////////////////////////////////////
// OUR REFERENCE TO OURSELF. THIS IS AN ARRAY OF HTTPHEADER OBJECTS
////////////////////////////////////////////////////////////////////////////////
HTTPHeader::count	= 0;
HTTPHeader::headers	= nullptr;




////////////////////////////////////////////////////////////////////////////////
// CALCULATE THE TOTAL NUMBER OF HEADER ROWS
////////////////////////////////////////////////////////////////////////////////
static int HTTPHeader::_count(const char *buffer) {
	if (buffer == nullptr) return 0;

	int total = 0;

	while (*buffer) {
		if (buffer[0] == '\r'  &&  buffer[1] == '\n') {
			total++;
			buffer += 2;

			if (buffer[0] == '\r'  &&  buffer[1] == '\n') {
				break;
			}

		} else {
			buffer++;
		}
	}

	return total;
}




////////////////////////////////////////////////////////////////////////////////
// ARSE AND TOKENIZE THE BUFFER
////////////////////////////////////////////////////////////////////////////////
static char *HTTPHeader::_parse(char *buffer) {
	if (buffer == nullptr) return;

	const char *header_key		= buffer;
	const char *header_value	= nullptr;
	int			id				= 0;

	while (*buffer) {
		if (buffer[0] == '\r'  &&  buffer[1] == '\n') {
			*buffer++ = NULL;
			*buffer++ = NULL;

			//STORE THE HEADER LINE
			set(id++, header_key, header_value);

			//END OF HEADER SECTION
			if (buffer[0] == '\r'  &&  buffer[1] == '\n') {
				*buffer++ = NULL;
				*buffer++ = NULL;
				return buffer;
			}

			//RESET VALUES
			header_key		= buffer;
			header_value	= nullptr;

		} else if (*buffer == ':'  &&  header_value == nullptr) {
			do {
				*buffer++ = NULL;
			} while (*buffer == ' ');

			header_value = buffer++;

		} else {
			buffer++;
		}
	}

	return buffer;
}



////////////////////////////////////////////////////////////////////////////////
// PROCESS THE BUFFER
////////////////////////////////////////////////////////////////////////////////
static char *HTTPHeader::process(char *buffer) {
	if (headers) {
		delete[] headers;
		headers = nullptr;
	}

	count = _count(buffer);

	if (!count) return;

	headers = new HTTPHeader [count];

	_parse(_buffer);
}
