// B32CC — native C compiler for FPGC
//
// This wraps smlrc.c + cgb32p3.inc as a userBDOS program.
// B32CC auto-defines __SMALLER_C__ which activates the self-hosting code paths,
// and __WORD_ADDRESSABLE__ which sets CHAR_BIT=32 for correct buffer sizing.
//
// Compile with:
//   make fnp-upload-userbdos file=b32cc
//
// smlrc.c's __SMALLER_C__ block has #ifndef guards for macros also defined by
// the common libs (NULL, size_t, FILE, EOF, EXIT_FAILURE, fpos_t).
// Function re-declarations are harmless (B32CC ignores const qualifiers).

// BDOS syscall wrappers (needed by stdio.c for filesystem operations)
#define USER_SYSCALL
#include "libs/user/user.h"

// Common libraries (string, stdlib, ctype, stdio)
#define COMMON_STRING
#define COMMON_STDLIB
#define COMMON_CTYPE
#define COMMON_STDIO
#include "libs/common/common.h"

// Pull in the compiler source
#include "../../BuildTools/B32CC/smlrc.c"
