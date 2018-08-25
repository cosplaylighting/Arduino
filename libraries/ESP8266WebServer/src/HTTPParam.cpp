

#include <Arduino.h>
#include "HTTPParam.h"



////////////////////////////////////////////////////////////////////////////////
// CALCULATE THE TOTAL NUMBER OF HEADER ROWS
////////////////////////////////////////////////////////////////////////////////
int HTTPParam::_count(const char *buffer) const {
	if (buffer == nullptr  ||  *buffer == NULL) return 0;

	int total = 1;

	while (*buffer) {
		if (*buffer == '&') total++;
		buffer++;
	}

	return total;
}




////////////////////////////////////////////////////////////////////////////////
// PARSE AND TOKENIZE THE BUFFER
////////////////////////////////////////////////////////////////////////////////
char *HTTPParam::_parse(char *buffer) {
	if (buffer == nullptr) return buffer;

	char	*param_key		= buffer;
	char	*param_value	= nullptr;
	int		 id				= 0;

	while (true) {
		if (*buffer == '&'  ||  *buffer == NULL) {
			bool end = (*buffer == NULL);

			*buffer++ = NULL;

			set(id++, _decode(param_key), _decode(param_value));

			if (end) break;

			param_key	= buffer;
			param_value	= nullptr;

		} else if (*buffer == '='  &&  param_value == nullptr) {
			*buffer++ = NULL;
			param_value = buffer;

		} else {
			buffer++;
		}
	}

	return buffer;
}




////////////////////////////////////////////////////////////////////////////////
// DECODE URL ENCODING - FAST INLINE MEMORY REPLACE WITHIN THE STRING
////////////////////////////////////////////////////////////////////////////////
char *HTTPParam::_decode(char *buffer) {
	if (buffer == nullptr) return buffer;

	char *src	= buffer;
	char *dst	= buffer;
	char temp[]	= "00";

	while (*src) {
		switch (*src) {
			case '%':
				if (!src[1]  ||  !src[2]) goto finished;

				temp[0] = src[1];
				temp[1] = src[2];

				src += 2;

				*dst = (char) strtol(temp, nullptr, 16);
				if (*dst == NULL) *dst = ' ';
			break;


			case '+':
				*dst = ' ';
			break;


			default:
				*dst = *src;
		}

		src++;
		dst++;
	}


finished:
	*dst = NULL;

	return buffer;
}
