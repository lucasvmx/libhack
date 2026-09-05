#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include "init.h"
#include "process.h"
#include "status_codes.h"
#include "types.h"

static unsigned tests_run;
static unsigned tests_failed;

#define EXPECT(condition)                                                       \
    do                                                                          \
    {                                                                           \
        tests_run++;                                                            \
        if (!(condition))                                                       \
        {                                                                       \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__,          \
                    #condition);                                               \
            tests_failed++;                                                     \
        }                                                                       \
    } while (0)

static void test_status_codes(void)
{
    EXPECT(libhack_get_last_error() == LIBHACK_OK);

    libhack_set_last_error(42);
    EXPECT(libhack_get_last_error() == 42);

    libhack_set_last_error(-7);
    EXPECT(libhack_get_last_error() == -7);

    libhack_set_last_error(LIBHACK_OK);
}

static void test_string_lowercase(void)
{
#ifndef __windows__
    char text[] = "LiBhAcK 123!";

    EXPECT(strcmp(strlwr(text), "libhack 123!") == 0);
    EXPECT(strlwr(text) == text);
#endif
}

static void test_initialization(void)
{
    struct libhack_handle *handle;

    EXPECT(libhack_init(NULL) == NULL);

    handle = libhack_init("example-process");
    EXPECT(handle != NULL);

    if (handle == NULL)
        return;

#ifdef __linux__
    EXPECT(strcmp(handle->process_name, "example-process") == 0);
    EXPECT(handle->pid == -1);
    EXPECT(handle->base_addr == -1);
#else
    EXPECT(strcmp(handle->process_name, "example-process") == 0);
    EXPECT(handle->pid == 0);
    EXPECT(handle->bProcessIsOpen == FALSE);
#endif

    libhack_free(handle);
    libhack_free(NULL);
}

static void test_version(void)
{
    const char *version = libhack_getversion();

    EXPECT(version != NULL);
    EXPECT(version[0] != '\0');
    EXPECT(strstr(version, "build") != NULL);
}

#ifdef __linux__
static bool read_current_process_name(char *name, size_t name_size)
{
    FILE *comm_file;

    if (name == NULL || name_size < 2)
        return false;

    comm_file = fopen("/proc/self/comm", "r");
    if (comm_file == NULL)
        return false;

    if (fgets(name, (int)name_size, comm_file) == NULL)
    {
        fclose(comm_file);
        return false;
    }

    fclose(comm_file);
    name[strcspn(name, "\r\n")] = '\0';
    return name[0] != '\0';
}

static void test_process_lookup_and_state(void)
{
    char process_name[BUFLEN] = {0};
    struct libhack_handle *handle;
    pid_t pid;

    EXPECT(read_current_process_name(process_name, sizeof(process_name)));
    if (process_name[0] == '\0')
        return;

    handle = libhack_init(process_name);
    EXPECT(handle != NULL);
    if (handle == NULL)
        return;

    EXPECT(libhack_get_process_id(NULL) == -1);
    pid = libhack_get_process_id(handle);
    EXPECT(pid == getpid());
    EXPECT(handle->pid == getpid());
    EXPECT(libhack_get_process_id(handle) == getpid());
    EXPECT(libhack_process_is_running(handle));

    libhack_free(handle);
}

static void test_process_memory(void)
{
    char process_name[BUFLEN] = {0};
    char target_string[32] = "before";
    const char replacement[] = "after";
    int target_int = 1234;
    int read_int = 0;
    int64_t target_int64 = INT64_C(0x1020304050607080);
    struct libhack_handle *handle;
    long base_address;
    long read_result;

    EXPECT(read_current_process_name(process_name, sizeof(process_name)));
    if (process_name[0] == '\0')
        return;

    handle = libhack_init(process_name);
    EXPECT(handle != NULL);
    if (handle == NULL)
        return;

    EXPECT(libhack_read_int_from_addr64(NULL, 0, &read_int) == -1);
    EXPECT(libhack_read_int_from_addr64(handle, 0, NULL) == -1);
    EXPECT(libhack_write_int_to_addr64(NULL, 0, 1) == -1);
    EXPECT(libhack_write_string_to_addr64(NULL, 0, replacement,
                                          sizeof(replacement)) == -1);
    EXPECT(libhack_read_int64_from_addr64(NULL, 0) == -1);
    EXPECT(libhack_get_process_id(handle) == getpid());

    read_result = libhack_read_int_from_addr64(
        handle, (DWORD64)(uintptr_t)&target_int, &read_int);
    if (read_result == EPERM || read_result == EACCES || read_result == ENOSYS)
    {
        printf("Operações process_vm_readv/process_vm_writev ignoradas: %s.\n",
               strerror((int)read_result));
        libhack_free(handle);
        return;
    }

    EXPECT(read_result == LIBHACK_OK);
    EXPECT(read_int == target_int);

    base_address = libhack_get_base_addr(handle);
    EXPECT(base_address > 0);
    EXPECT(handle->base_addr == base_address);
    EXPECT(libhack_get_base_addr64(handle) == base_address);

    EXPECT(libhack_write_int_to_addr64(
                handle, (DWORD64)(uintptr_t)&target_int, 5678) ==
            LIBHACK_OK);
    EXPECT(target_int == 5678);

    EXPECT(libhack_write_string_to_addr64(
                handle, (DWORD64)(uintptr_t)target_string, replacement,
                sizeof(replacement)) == LIBHACK_OK);
    EXPECT(strcmp(target_string, replacement) == 0);

    EXPECT(libhack_read_int64_from_addr64(
                handle, (DWORD64)(uintptr_t)&target_int64) == target_int64);

    libhack_free(handle);
}
#endif

int main(void)
{
    test_status_codes();
    test_string_lowercase();
    test_initialization();
    test_version();

#ifdef __linux__
    test_process_lookup_and_state();
    test_process_memory();
#endif

    if (tests_failed != 0)
    {
        fprintf(stderr, "%u de %u verificações falharam.\n", tests_failed,
                tests_run);
        return EXIT_FAILURE;
    }

    printf("%u verificações passaram.\n", tests_run);
    return EXIT_SUCCESS;
}
