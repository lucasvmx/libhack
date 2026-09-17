#ifndef LIBHACK_TYPES_H
#define LIBHACK_TYPES_H

#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include "platform.h"

#if defined(__linux32) || defined(__linux64)
    #ifndef DWORD
        #define DWORD unsigned long
        #define DWORD64 unsigned long long
    #endif
#endif

typedef uintptr_t libhack_address_t;
typedef intptr_t libhack_offset_t;
typedef uint64_t libhack_pid_t;

#define LIBHACK_PROTECTION_READ    UINT32_C(0x01)
#define LIBHACK_PROTECTION_WRITE   UINT32_C(0x02)
#define LIBHACK_PROTECTION_EXECUTE UINT32_C(0x04)
#define LIBHACK_PROTECTION_GUARD   UINT32_C(0x08)

#ifndef __windows__
/**
 * @brief converts a string to lowercase
 * 
 * @param str 
 * @return char* 
 */
extern char *strlwr(char *);
#endif

#endif
