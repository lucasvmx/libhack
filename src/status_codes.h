#ifndef LIBHACK_STATUS_CODES_H
#define LIBHACK_STATUS_CODES_H

#include <sys/types.h>

#define LIBHACK_OK 0

__int32_t libhack_get_last_error();
void libhack_set_last_error(__int32_t err);

#endif
