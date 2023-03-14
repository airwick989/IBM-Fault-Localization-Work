
#include <ctype.h>
#include <stdio.h>

#include "events.h"
#include "utils.h"

pi_event get_pi_event_from_name(char *s)
{
   return INVALID_PI_EVENT;
}

unsigned long parse_long(char *intStr) {
	char *c = intStr;
	unsigned long ulong = 0;
	while (c && *c >= '0' && *c <= '9') {
		ulong = (ulong * 10) + (*c - 48);
		c++;
	}
	printf("%s : %lu\n",intStr,ulong);
	return ulong;
}

void strToUpper(char *str) {
	char temp = 0;
	while (str && *str) {
		temp = toupper(*str);
		if (temp)
			*str = temp;
		str++;
	}
}
