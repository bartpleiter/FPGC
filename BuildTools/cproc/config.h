/* B32P3 cross-compiler configuration */
static char *preprocesscmd[] = {"cpp", "-P", 0};
static char *codegencmd[] = {"qbe", 0};
static char *assemblecmd[] = {"as", 0};
static char *linkcmd[] = {"ld", 0};
static char *startfiles[] = {0};
static char *endfiles[] = {0};
static char *defines[] = {
	"-D", "__b32p3__",
	"-D", "__B32P3__",
	0
};
static char *target = "b32p3";
