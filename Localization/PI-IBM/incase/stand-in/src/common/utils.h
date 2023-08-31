#ifndef utils_h
#define utils_h

pi_event get_pi_event_from_name(char *s);

/**
 * Reads a string starting at pointer and attempts parse out an integer
 * stops reading if characters are not numbers or hits end of string
 * returns 0 if nothing is found
 * can overflow if number is too long
 */
unsigned long parse_long(char *intStr);

/**
 * converts string characters to upper. changes are made in place and not
 * copied to a new string
 */
void strToUpper(char *str);

#endif
