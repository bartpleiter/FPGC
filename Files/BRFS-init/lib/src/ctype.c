#include <ctype.h>

#define _U  0x01  /* upper */
#define _L  0x02  /* lower */
#define _D  0x04  /* digit */
#define _S  0x08  /* space (isspace) */
#define _P  0x10  /* punct */
#define _C  0x20  /* control */
#define _X  0x40  /* hex digit (a-f, A-F) */
#define _B  0x80  /* blank (space, tab) */

static const unsigned char ctype_table[128] = {
    /* 0x00 NUL */ _C,
    /* 0x01 SOH */ _C,
    /* 0x02 STX */ _C,
    /* 0x03 ETX */ _C,
    /* 0x04 EOT */ _C,
    /* 0x05 ENQ */ _C,
    /* 0x06 ACK */ _C,
    /* 0x07 BEL */ _C,
    /* 0x08 BS  */ _C,
    /* 0x09 HT  */ _C | _S | _B,
    /* 0x0A LF  */ _C | _S,
    /* 0x0B VT  */ _C | _S,
    /* 0x0C FF  */ _C | _S,
    /* 0x0D CR  */ _C | _S,
    /* 0x0E SO  */ _C,
    /* 0x0F SI  */ _C,
    /* 0x10 DLE */ _C,
    /* 0x11 DC1 */ _C,
    /* 0x12 DC2 */ _C,
    /* 0x13 DC3 */ _C,
    /* 0x14 DC4 */ _C,
    /* 0x15 NAK */ _C,
    /* 0x16 SYN */ _C,
    /* 0x17 ETB */ _C,
    /* 0x18 CAN */ _C,
    /* 0x19 EM  */ _C,
    /* 0x1A SUB */ _C,
    /* 0x1B ESC */ _C,
    /* 0x1C FS  */ _C,
    /* 0x1D GS  */ _C,
    /* 0x1E RS  */ _C,
    /* 0x1F US  */ _C,
    /* 0x20 SP  */ _S | _B,
    /* 0x21 !   */ _P,
    /* 0x22 "   */ _P,
    /* 0x23 #   */ _P,
    /* 0x24 $   */ _P,
    /* 0x25 %   */ _P,
    /* 0x26 &   */ _P,
    /* 0x27 '   */ _P,
    /* 0x28 (   */ _P,
    /* 0x29 )   */ _P,
    /* 0x2A *   */ _P,
    /* 0x2B +   */ _P,
    /* 0x2C ,   */ _P,
    /* 0x2D -   */ _P,
    /* 0x2E .   */ _P,
    /* 0x2F /   */ _P,
    /* 0x30 0   */ _D,
    /* 0x31 1   */ _D,
    /* 0x32 2   */ _D,
    /* 0x33 3   */ _D,
    /* 0x34 4   */ _D,
    /* 0x35 5   */ _D,
    /* 0x36 6   */ _D,
    /* 0x37 7   */ _D,
    /* 0x38 8   */ _D,
    /* 0x39 9   */ _D,
    /* 0x3A :   */ _P,
    /* 0x3B ;   */ _P,
    /* 0x3C <   */ _P,
    /* 0x3D =   */ _P,
    /* 0x3E >   */ _P,
    /* 0x3F ?   */ _P,
    /* 0x40 @   */ _P,
    /* 0x41 A   */ _U | _X,
    /* 0x42 B   */ _U | _X,
    /* 0x43 C   */ _U | _X,
    /* 0x44 D   */ _U | _X,
    /* 0x45 E   */ _U | _X,
    /* 0x46 F   */ _U | _X,
    /* 0x47 G   */ _U,
    /* 0x48 H   */ _U,
    /* 0x49 I   */ _U,
    /* 0x4A J   */ _U,
    /* 0x4B K   */ _U,
    /* 0x4C L   */ _U,
    /* 0x4D M   */ _U,
    /* 0x4E N   */ _U,
    /* 0x4F O   */ _U,
    /* 0x50 P   */ _U,
    /* 0x51 Q   */ _U,
    /* 0x52 R   */ _U,
    /* 0x53 S   */ _U,
    /* 0x54 T   */ _U,
    /* 0x55 U   */ _U,
    /* 0x56 V   */ _U,
    /* 0x57 W   */ _U,
    /* 0x58 X   */ _U,
    /* 0x59 Y   */ _U,
    /* 0x5A Z   */ _U,
    /* 0x5B [   */ _P,
    /* 0x5C \   */ _P,
    /* 0x5D ]   */ _P,
    /* 0x5E ^   */ _P,
    /* 0x5F _   */ _P,
    /* 0x60 `   */ _P,
    /* 0x61 a   */ _L | _X,
    /* 0x62 b   */ _L | _X,
    /* 0x63 c   */ _L | _X,
    /* 0x64 d   */ _L | _X,
    /* 0x65 e   */ _L | _X,
    /* 0x66 f   */ _L | _X,
    /* 0x67 g   */ _L,
    /* 0x68 h   */ _L,
    /* 0x69 i   */ _L,
    /* 0x6A j   */ _L,
    /* 0x6B k   */ _L,
    /* 0x6C l   */ _L,
    /* 0x6D m   */ _L,
    /* 0x6E n   */ _L,
    /* 0x6F o   */ _L,
    /* 0x70 p   */ _L,
    /* 0x71 q   */ _L,
    /* 0x72 r   */ _L,
    /* 0x73 s   */ _L,
    /* 0x74 t   */ _L,
    /* 0x75 u   */ _L,
    /* 0x76 v   */ _L,
    /* 0x77 w   */ _L,
    /* 0x78 x   */ _L,
    /* 0x79 y   */ _L,
    /* 0x7A z   */ _L,
    /* 0x7B {   */ _P,
    /* 0x7C |   */ _P,
    /* 0x7D }   */ _P,
    /* 0x7E ~   */ _P,
    /* 0x7F DEL */ _C,
};

static int
ctype_get(int c)
{
    if (c < 0 || c > 127)
        return 0;
    return ctype_table[c];
}

int isalnum(int c)  { return ctype_get(c) & (_U | _L | _D); }
int isalpha(int c)  { return ctype_get(c) & (_U | _L); }
int isblank(int c)  { return ctype_get(c) & _B; }
int iscntrl(int c)  { return ctype_get(c) & _C; }
int isdigit(int c)  { return ctype_get(c) & _D; }
int isgraph(int c)  { return ctype_get(c) & (_U | _L | _D | _P); }
int islower(int c)  { return ctype_get(c) & _L; }
int isprint(int c)  { return ctype_get(c) & (_U | _L | _D | _P | _B); }
int ispunct(int c)  { return ctype_get(c) & _P; }
int isspace(int c)  { return ctype_get(c) & _S; }
int isupper(int c)  { return ctype_get(c) & _U; }
int isxdigit(int c) { return ctype_get(c) & (_D | _X); }
int isascii(int c)  { return (unsigned)c <= 127; }
int toascii(int c)  { return c & 0x7f; }

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');
    return c;
}
