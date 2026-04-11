#include <getopt.h>
#include <string.h>
#include <stdio.h>

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

static int optpos = 1;  /* position within current argv element */

int
getopt(int argc, char * const argv[], const char *optstring)
{
    const char *p;
    const char *arg;

    optarg = NULL;

    if (optind >= argc)
        return -1;

    arg = argv[optind];

    /* Not an option */
    if (arg == NULL || arg[0] != '-' || arg[1] == '\0')
        return -1;

    /* "--" terminates options */
    if (arg[1] == '-' && arg[2] == '\0') {
        optind++;
        return -1;
    }

    optopt = arg[optpos];
    p = strchr(optstring, optopt);

    if (p == NULL || optopt == ':') {
        /* Unknown option */
        if (opterr)
            fprintf(stderr, "unknown option: -%c\n", optopt);
        if (arg[++optpos] == '\0') {
            optind++;
            optpos = 1;
        }
        return '?';
    }

    if (p[1] == ':') {
        /* Option requires an argument */
        if (arg[optpos + 1] != '\0') {
            /* Argument is rest of current argv element */
            optarg = (char *)&arg[optpos + 1];
        } else if (optind + 1 < argc) {
            /* Argument is next argv element */
            optarg = argv[++optind];
        } else {
            /* Missing argument */
            if (opterr)
                fprintf(stderr, "option -%c requires an argument\n", optopt);
            optind++;
            optpos = 1;
            return (optstring[0] == ':') ? ':' : '?';
        }
        optind++;
        optpos = 1;
    } else {
        /* No argument */
        if (arg[++optpos] == '\0') {
            optind++;
            optpos = 1;
        }
    }

    return optopt;
}
