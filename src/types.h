#ifndef LIBHACK_TYPES_H
#define LIBHACK_TYPES_H

#include <ctype.h>
#include "platform.h"

#if defined(__linux32) || defined(__linux64)
#define DWORD unsigned long
#define DWORD64 unsigned long long
typedef void *PVOID;
typedef PVOID HANDLE;
#endif

/**
 * @brief converts a string to lowercase
 * 
 * @param str 
 * @return char* 
 */
extern char *strlwr(char *);

#endif
