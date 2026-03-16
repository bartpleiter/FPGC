#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
void __assert_fail(const char *expr, const char *file, int line);
#define assert(expression) \
    ((expression) ? (void)0 : __assert_fail(#expression, __FILE__, __LINE__))
#endif
