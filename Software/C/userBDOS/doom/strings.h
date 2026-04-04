/* Stub strings.h for FPGC — provides strcasecmp */
#ifndef _STRINGS_H
#define _STRINGS_H
#include <string.h>

/* Case-insensitive string comparison */
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, unsigned int n);

#endif
