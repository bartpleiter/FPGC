#ifndef CTYPE_H
#define CTYPE_H

// Character Classification Functions

// Check if character is a digit (0-9).
int isdigit(int c);

// Check if character is alphabetic (a-z, A-Z).
int isalpha(int c);

// Check if character is alphanumeric (a-z, A-Z, 0-9).
int isalnum(int c);

// Check if character is whitespace (space, tab, newline, etc.).
int isspace(int c);

// Check if character is uppercase (A-Z).
int isupper(int c);

// Check if character is lowercase (a-z).
int islower(int c);

// Check if character is a hexadecimal digit (0-9, a-f, A-F).
int isxdigit(int c);

// Check if character is printable (including space).
int isprint(int c);

// Check if character is a control character.
int iscntrl(int c);

// Check if character is punctuation.
int ispunct(int c);

// Check if character has graphical representation (excludes space).
int isgraph(int c);

// Convert character to uppercase.
int toupper(int c);

// Convert character to lowercase.
int tolower(int c);

#endif // CTYPE_H
