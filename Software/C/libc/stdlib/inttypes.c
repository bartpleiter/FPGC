#include <inttypes.h>

intmax_t imaxabs(intmax_t j) {
    return j < 0 ? -j : j;
}

imaxdiv_t imaxdiv(intmax_t n, intmax_t d) {
    imaxdiv_t r;
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

intmax_t strtoimax(const char *nptr, char **endptr, int base) {
    return (intmax_t)strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base) {
    return (uintmax_t)strtoull(nptr, endptr, base);
}
