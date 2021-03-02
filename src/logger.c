
#include <stdio.h>
#include <stdarg.h>
#include "logger.h"

#if defined(DEBUG) || !defined(NDEBUG)
void libhack_debug(const char *msg, ...)
{
    va_list list;

    va_start(list, msg);
    fprintf(stdout, "[LIBHACK DEBUG] ");
    vfprintf(stdout, msg, list);
    fprintf(stdout, "\n");
    va_end(list);
}

void libhack_warn(const char *msg, ...)
{
    va_list list;

    va_start(list, msg);
    fprintf(stdout, "[LIBHACK WARNING] ");
    vfprintf(stdout, msg, list);
    fprintf(stdout, "\n");
    va_end(list);
}

void libhack_notice(const char *msg, ...)
{
    va_list list;

    va_start(list, msg);
    fprintf(stdout, "[LIBHACK INFO] ");
    vfprintf(stdout, msg, list);
    fprintf(stdout, "\n");
    va_end(list);
}

void libhack_err(const char *msg, ...)
{
    va_list list;

    va_start(list, msg);
    fprintf(stdout, "[LIBHACK ERROR] ");
    vfprintf(stdout, msg, list);
    fprintf(stdout, "\n");
    va_end(list);
}
#endif
