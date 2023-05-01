#ifndef LIBHACK_STATUS_CODES_H
#define LIBHACK_STATUS_CODES_H

#include "platform.h"

#ifdef __windows__
#include <stdint.h>
#elif __linux__
#include <sys/types.h>
#endif

#define LIBHACK_OK 0

#ifdef __windows__
typedef int32_t __int32_t;
#endif

__int32_t libhack_get_last_error();
void libhack_set_last_error(__int32_t err);


#endif
