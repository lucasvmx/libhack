#ifndef LIBHACK_STATUS_CODES_H
#define LIBHACK_STATUS_CODES_H

#include "platform.h"
#include <stdint.h>

#ifdef __linux__
#include <sys/types.h>
#endif

typedef enum libhack_status
{
    LIBHACK_OK = 0,
    LIBHACK_INVALID_ARGUMENT = 1,
    LIBHACK_NOT_FOUND = 2,
    LIBHACK_ACCESS_DENIED = 3,
    LIBHACK_INVALID_ADDRESS = 4,
    LIBHACK_PARTIAL_TRANSFER = 5,
    LIBHACK_OVERFLOW = 6,
    LIBHACK_UNSUPPORTED = 7,
    LIBHACK_NATIVE_ERROR = 8,
    LIBHACK_OUT_OF_MEMORY = 9
} libhack_status_t;

#ifndef LIBHACK_API
#if defined(__windows__) && defined(DLL_EXPORT)
#define LIBHACK_API __declspec(dllexport)
#else
#define LIBHACK_API
#endif
#endif

LIBHACK_API int32_t libhack_get_last_error(void);
LIBHACK_API void libhack_set_last_error(int32_t err);
LIBHACK_API int32_t libhack_get_last_native_error(void);
LIBHACK_API void libhack_set_last_native_error(int32_t err);


#endif
