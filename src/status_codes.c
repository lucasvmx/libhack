#include "platform.h"
#include "status_codes.h"
#include "logger.h"

#ifdef __linux__
#include <pthread.h>
#include <stdbool.h>
#elif __windows__
#include <stdint.h>
#endif

#ifdef __linux__
static pthread_mutex_t mux;
static pthread_once_t pot = PTHREAD_ONCE_INIT;
#endif

int32_t last_error = LIBHACK_OK;

static void init_mux() {
#ifdef __linux__
    if(pthread_mutex_init(&mux, NULL) != 0) {
        libhack_err("get last error and set last error should not be used in multiple threads");
        return;
    }
#elif __windows__

#endif
}

static void initialize_error_system() {
#ifdef __linux__
    pthread_once(pot, init_mux);
#elif __windows

#endif
}

__int32_t libhack_get_last_error()
{
    int32_t err;

    initialize_error_system();

#ifdef __linux__
    pthread_mutex_lock(&mux);
    err = last_error;
    pthread_mutex_unlock(&mux);
#elif __windows__

#endif

    return err;
}

void libhack_set_last_error(__int32_t err)
{
    initialize_error_system();

#ifdef __linux__
    pthread_mutex_lock(&mux);
    last_error = err;
    pthread_mutex_unlock(&mux);
#elif __windows__

#endif
}

