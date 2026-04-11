#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>

/* Format macros for fprintf — B32P3 is ILP32, long long is 64-bit */

/* Signed decimal */
#define PRId8    "d"
#define PRId16   "d"
#define PRId32   "d"
#define PRId64   "lld"
#define PRIdLEAST8  "d"
#define PRIdLEAST16 "d"
#define PRIdLEAST32 "d"
#define PRIdLEAST64 "lld"
#define PRIdFAST8   "d"
#define PRIdFAST16  "d"
#define PRIdFAST32  "d"
#define PRIdFAST64  "lld"
#define PRIdMAX     "lld"
#define PRIdPTR     "d"

/* Signed integer */
#define PRIi8    "i"
#define PRIi16   "i"
#define PRIi32   "i"
#define PRIi64   "lli"
#define PRIiLEAST8  "i"
#define PRIiLEAST16 "i"
#define PRIiLEAST32 "i"
#define PRIiLEAST64 "lli"
#define PRIiFAST8   "i"
#define PRIiFAST16  "i"
#define PRIiFAST32  "i"
#define PRIiFAST64  "lli"
#define PRIiMAX     "lli"
#define PRIiPTR     "i"

/* Unsigned octal */
#define PRIo8    "o"
#define PRIo16   "o"
#define PRIo32   "o"
#define PRIo64   "llo"
#define PRIoLEAST8  "o"
#define PRIoLEAST16 "o"
#define PRIoLEAST32 "o"
#define PRIoLEAST64 "llo"
#define PRIoFAST8   "o"
#define PRIoFAST16  "o"
#define PRIoFAST32  "o"
#define PRIoFAST64  "llo"
#define PRIoMAX     "llo"
#define PRIoPTR     "o"

/* Unsigned decimal */
#define PRIu8    "u"
#define PRIu16   "u"
#define PRIu32   "u"
#define PRIu64   "llu"
#define PRIuLEAST8  "u"
#define PRIuLEAST16 "u"
#define PRIuLEAST32 "u"
#define PRIuLEAST64 "llu"
#define PRIuFAST8   "u"
#define PRIuFAST16  "u"
#define PRIuFAST32  "u"
#define PRIuFAST64  "llu"
#define PRIuMAX     "llu"
#define PRIuPTR     "u"

/* Unsigned hex (lowercase) */
#define PRIx8    "x"
#define PRIx16   "x"
#define PRIx32   "x"
#define PRIx64   "llx"
#define PRIxLEAST8  "x"
#define PRIxLEAST16 "x"
#define PRIxLEAST32 "x"
#define PRIxLEAST64 "llx"
#define PRIxFAST8   "x"
#define PRIxFAST16  "x"
#define PRIxFAST32  "x"
#define PRIxFAST64  "llx"
#define PRIxMAX     "llx"
#define PRIxPTR     "x"

/* Unsigned hex (uppercase) */
#define PRIX8    "X"
#define PRIX16   "X"
#define PRIX32   "X"
#define PRIX64   "llX"
#define PRIXLEAST8  "X"
#define PRIXLEAST16 "X"
#define PRIXLEAST32 "X"
#define PRIXLEAST64 "llX"
#define PRIXFAST8   "X"
#define PRIXFAST16  "X"
#define PRIXFAST32  "X"
#define PRIXFAST64  "llX"
#define PRIXMAX     "llX"
#define PRIXPTR     "X"

/* Scan macros for fscanf — same mappings */
#define SCNd8    "hhd"
#define SCNd16   "hd"
#define SCNd32   "d"
#define SCNd64   "lld"
#define SCNi8    "hhi"
#define SCNi16   "hi"
#define SCNi32   "i"
#define SCNi64   "lli"
#define SCNo8    "hho"
#define SCNo16   "ho"
#define SCNo32   "o"
#define SCNo64   "llo"
#define SCNu8    "hhu"
#define SCNu16   "hu"
#define SCNu32   "u"
#define SCNu64   "llu"
#define SCNx8    "hhx"
#define SCNx16   "hx"
#define SCNx32   "x"
#define SCNx64   "llx"

/* imaxabs / imaxdiv */
typedef struct {
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

intmax_t imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t n, intmax_t d);

/* strtoimax / strtoumax */
#include <stdlib.h>
intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);

#endif /* _INTTYPES_H */
