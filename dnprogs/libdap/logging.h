// libdap/logging.h
//
#include <stdarg.h>

void init_logging(const char *, char, bool);
void daplog(int level, const char *fmt, ...);

#define DAPLOG(x) daplog x
