#ifndef CTYPE_H
#define CTYPE_H

/*
 * Character Classification Functions
 * Minimal implementation for FPGC.
 */

/**
 * Check if character is a digit (0-9).
 * @param c Character to check
 * @return Non-zero if digit, zero otherwise
 */
int isdigit(int c);

/**
 * Check if character is alphabetic (a-z, A-Z).
 * @param c Character to check
 * @return Non-zero if alphabetic, zero otherwise
 */
int isalpha(int c);

/**
 * Check if character is alphanumeric (a-z, A-Z, 0-9).
 * @param c Character to check
 * @return Non-zero if alphanumeric, zero otherwise
 */
int isalnum(int c);

/**
 * Check if character is whitespace (space, tab, newline, etc.).
 * @param c Character to check
 * @return Non-zero if whitespace, zero otherwise
 */
int isspace(int c);

/**
 * Check if character is uppercase (A-Z).
 * @param c Character to check
 * @return Non-zero if uppercase, zero otherwise
 */
int isupper(int c);

/**
 * Check if character is lowercase (a-z).
 * @param c Character to check
 * @return Non-zero if lowercase, zero otherwise
 */
int islower(int c);

/**
 * Check if character is a hexadecimal digit (0-9, a-f, A-F).
 * @param c Character to check
 * @return Non-zero if hex digit, zero otherwise
 */
int isxdigit(int c);

/**
 * Check if character is printable (including space).
 * @param c Character to check
 * @return Non-zero if printable, zero otherwise
 */
int isprint(int c);

/**
 * Check if character is a control character.
 * @param c Character to check
 * @return Non-zero if control character, zero otherwise
 */
int iscntrl(int c);

/**
 * Check if character is punctuation.
 * @param c Character to check
 * @return Non-zero if punctuation, zero otherwise
 */
int ispunct(int c);

/**
 * Check if character has graphical representation (excludes space).
 * @param c Character to check
 * @return Non-zero if graphical, zero otherwise
 */
int isgraph(int c);

/**
 * Convert character to uppercase.
 * @param c Character to convert
 * @return Uppercase version, or c if not lowercase
 */
int toupper(int c);

/**
 * Convert character to lowercase.
 * @param c Character to convert
 * @return Lowercase version, or c if not uppercase
 */
int tolower(int c);

#endif /* CTYPE_H */
