#ifndef LIBHACK_STATUS_CODES_H
#define LIBHACK_STATUS_CODES_H

#include "platform.h"
#include <stdint.h>

#ifdef __linux__
#include <sys/types.h>
#endif

#define LIBHACK_OK 0

#ifndef LIBHACK_API
#if defined(__windows__) && defined(DLL_EXPORT)
#define LIBHACK_API __declspec(dllexport)
#else
#define LIBHACK_API
#endif
#endif

LIBHACK_API int32_t libhack_get_last_error(void);
LIBHACK_API void libhack_set_last_error(int32_t err);


#endif
