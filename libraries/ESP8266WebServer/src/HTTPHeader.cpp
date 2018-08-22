

#include <Arduino.h>
#include "HTTPHeader.h"




////////////////////////////////////////////////////////////////////////////////
// OUR REFERENCE TO OURSELF. THIS IS AN ARRAY OF HTTPHEADER OBJECTS
////////////////////////////////////////////////////////////////////////////////
int			HTTPHeader::count	= 0;
HTTPHeader *HTTPHeader::headers	= nullptr;




////////////////////////////////////////////////////////////////////////////////
// CALCULATE THE TOTAL NUMBER OF HEADER ROWS
////////////////////////////////////////////////////////////////////////////////
int HTTPHeader::_count(const char *buffer) {
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
// PARSE AND TOKENIZE THE BUFFER
////////////////////////////////////////////////////////////////////////////////
char *HTTPHeader::_parse(char *buffer) {
	if (buffer == nullptr) return buffer;

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
char *HTTPHeader::process(char *buffer) {
	if (headers) {
		delete[] headers;
		headers = nullptr;
	}

	count = _count(buffer);

	if (!count) return buffer;

	headers = new HTTPHeader [count];

	return _parse(buffer);
}




////////////////////////////////////////////////////////////////////////////////
// GET A HEADER BASED ON NAME
////////////////////////////////////////////////////////////////////////////////
HTTPHeader *HTTPHeader::get(const char *name) {
	if (name == nullptr  ||  *name == NULL) return nullptr;

	for (auto i=0; i<count; i++) {
		if (strcasecmp(headers[i].key, name) == 0) {
			return &headers[i];
		}
	}

	return nullptr;
}
