//
// ctype library implementation.
//

#include "libs/common/ctype.h"

// Return non-zero when c is a decimal digit.
int isdigit(int c)
{
  return (c >= '0' && c <= '9');
}

// Return non-zero when c is an ASCII alphabetic character.
int isalpha(int c)
{
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

// Return non-zero when c is alphanumeric.
int isalnum(int c)
{
  return isdigit(c) || isalpha(c);
}

// Return non-zero when c is an ASCII whitespace character.
int isspace(int c)
{
  return (c == ' ' || c == '\t' || c == '\n' ||
          c == '\r' || c == '\f' || c == '\v');
}

// Return non-zero when c is an uppercase ASCII letter.
int isupper(int c)
{
  return (c >= 'A' && c <= 'Z');
}

// Return non-zero when c is a lowercase ASCII letter.
int islower(int c)
{
  return (c >= 'a' && c <= 'z');
}

// Return non-zero when c is a hexadecimal digit.
int isxdigit(int c)
{
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Return non-zero when c is a printable ASCII character.
int isprint(int c)
{
  return (c >= 0x20 && c <= 0x7E);
}

// Return non-zero when c is an ASCII control character.
int iscntrl(int c)
{
  return (c >= 0x00 && c <= 0x1F) || c == 0x7F;
}

// Return non-zero when c is printable punctuation.
int ispunct(int c)
{
  return isprint(c) && !isalnum(c) && !isspace(c);
}

// Return non-zero when c is printable and not a space.
int isgraph(int c)
{
  return isprint(c) && c != ' ';
}

// Convert lowercase ASCII to uppercase; leave other values unchanged.
int toupper(int c)
{
  if (islower(c))
  {
    return c - ('a' - 'A');
  }
  return c;
}

// Convert uppercase ASCII to lowercase; leave other values unchanged.
int tolower(int c)
{
  if (isupper(c))
  {
    return c + ('a' - 'A');
  }
  return c;
}
