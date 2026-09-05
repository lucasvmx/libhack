#include "platform.h"
#include "status_codes.h"
#include "logger.h"

#ifdef __linux__
#include <pthread.h>
#endif

static int32_t last_error = LIBHACK_OK;

#ifdef __linux__
static pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;
#endif

int32_t libhack_get_last_error(void)
{
    int32_t err;

#ifdef __linux__
    pthread_mutex_lock(&mux);
    err = last_error;
    pthread_mutex_unlock(&mux);
#else
    err = last_error;
#endif

    return err;
}

void libhack_set_last_error(int32_t err)
{
#ifdef __linux__
    pthread_mutex_lock(&mux);
    last_error = err;
    pthread_mutex_unlock(&mux);
#else
    last_error = err;
#endif
}
