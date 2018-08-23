

#include <Arduino.h>
#include "HTTPHeader.h"



////////////////////////////////////////////////////////////////////////////////
// CALCULATE THE TOTAL NUMBER OF HEADER ROWS
////////////////////////////////////////////////////////////////////////////////
int HTTPHeader::_count(const char *buffer) const {
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
