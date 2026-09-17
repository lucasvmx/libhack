#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || \
    defined(__MINGW64__)
#include <windows.h>
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

    libhack_set_last_native_error(1234);
    EXPECT(libhack_get_last_native_error() == 1234);

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
    EXPECT(libhack_init_by_pid(0) == NULL);

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
    /* A name can match another instance; verify liveness with an explicit PID. */
    {
        struct libhack_handle *pid_handle =
            libhack_init_by_pid((libhack_pid_t)getpid());
        EXPECT(pid_handle != NULL);
        EXPECT(pid_handle != NULL && libhack_process_is_running(pid_handle));
        libhack_free(pid_handle);
    }

    libhack_free(handle);
}

static void test_process_memory(void)
{
    char process_name[BUFLEN] = {0};
    char target_string[32] = "before";
    const char replacement[] = "after";
    int target_int = 1234;
    int read_int = 0;
    uintptr_t first_pointer;
    uintptr_t second_pointer;
    const DWORD64 pointer_offsets[] = {0, 0, 0};
    const DWORD64 overflow_offset[] = {1};
    DWORD64 resolved_address = 0;
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
    EXPECT(libhack_resolve_pointer_chain64(NULL, 0, pointer_offsets,
                                            arraySize(pointer_offsets),
                                            &resolved_address) == -1);
    EXPECT(libhack_resolve_pointer_chain64(handle, 0, NULL, 1,
                                            &resolved_address) == -1);
    EXPECT(libhack_resolve_pointer_chain64(handle, 0, pointer_offsets, 0,
                                            &resolved_address) == -1);
    EXPECT(libhack_resolve_pointer_chain64(handle, UINT64_MAX,
                                            overflow_offset, 1,
                                            &resolved_address) == EOVERFLOW);
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

    first_pointer = (uintptr_t)&second_pointer;
    second_pointer = (uintptr_t)&target_int;
    EXPECT(libhack_resolve_pointer_chain64(
                handle, (DWORD64)(uintptr_t)&first_pointer, pointer_offsets,
                arraySize(pointer_offsets), &resolved_address) == LIBHACK_OK);
    EXPECT(resolved_address == (DWORD64)(uintptr_t)&target_int);
    EXPECT(libhack_read_int_from_pointer_chain64(
                handle, (DWORD64)(uintptr_t)&first_pointer, pointer_offsets,
                arraySize(pointer_offsets), &read_int) == LIBHACK_OK);
    EXPECT(read_int == target_int);
    EXPECT(libhack_write_int_to_pointer_chain64(
                handle, (DWORD64)(uintptr_t)&first_pointer, pointer_offsets,
                arraySize(pointer_offsets), 2468) == LIBHACK_OK);
    EXPECT(target_int == 2468);

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

static void test_unified_memory_api(void)
{
    struct libhack_handle *handle;
    libhack_pid_t pid;
    uint8_t bytes[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    uint8_t scan_bytes[] = {0x60, 0xaa, 0x30, 0x60, 0xbb, 0x30};
    uint8_t read_bytes[sizeof(bytes)] = {0};
    const uint8_t pattern[] = {0x60, 0x00, 0x30};
    const char mask[] = "x?x";
    struct libhack_match_list matches = {0};
    struct libhack_memory_region_list regions = {0};
    struct libhack_module_list modules = {0};
    int target = 123;
    int replacement_target = 456;
    int read_target = 0;
    uintptr_t second_pointer = (uintptr_t)&target;
    uintptr_t first_pointer = (uintptr_t)&second_pointer;
    libhack_offset_t offsets[] = {0, 0, 0};
    struct
    {
        int before;
        int value;
    } negative_target = {7, 42};
    uintptr_t negative_pointer = (uintptr_t)&negative_target.value;
    libhack_offset_t negative_offsets[] = {0, -(libhack_offset_t)sizeof(int)};
    libhack_address_t resolved = 0;
    libhack_status_t status;

#ifdef __linux__
    pid = (libhack_pid_t)getpid();
#else
    pid = (libhack_pid_t)GetCurrentProcessId();
#endif

    handle = libhack_init_by_pid(pid);
    EXPECT(handle != NULL);
    if (handle == NULL)
        return;

    EXPECT(libhack_open_process(handle) == LIBHACK_OK);
    EXPECT(libhack_process_is_running(handle));
    EXPECT(libhack_read_memory(handle, (libhack_address_t)(uintptr_t)bytes,
                               NULL, 1) == LIBHACK_INVALID_ARGUMENT);
    EXPECT(libhack_write_memory(handle, (libhack_address_t)(uintptr_t)bytes,
                                NULL, 1) == LIBHACK_INVALID_ARGUMENT);
    status = libhack_read_memory(handle, (libhack_address_t)(uintptr_t)bytes,
                                 read_bytes, sizeof(read_bytes));
    if (status == LIBHACK_ACCESS_DENIED || status == LIBHACK_UNSUPPORTED)
    {
        EXPECT(libhack_get_last_native_error() == EPERM ||
               libhack_get_last_native_error() == EACCES ||
               status == LIBHACK_UNSUPPORTED);
        printf("API de memória unificada ignorada: %d.\n", status);
        libhack_close_process(handle);
        libhack_free(handle);
        return;
    }
    EXPECT(status == LIBHACK_OK);
    EXPECT(memcmp(bytes, read_bytes, sizeof(bytes)) == 0);

    EXPECT(libhack_write_memory(handle, (libhack_address_t)(uintptr_t)bytes,
                                "\x60\x70", 2) == LIBHACK_OK);
    EXPECT(bytes[0] == 0x60 && bytes[1] == 0x70);

    EXPECT(libhack_resolve_pointer_chain(
                handle, (libhack_address_t)(uintptr_t)&first_pointer, offsets,
                arraySize(offsets), &resolved) == LIBHACK_OK);
    EXPECT(resolved == (libhack_address_t)(uintptr_t)&target);
    EXPECT(libhack_read_pointer_chain(
                handle, (libhack_address_t)(uintptr_t)&first_pointer, offsets,
                arraySize(offsets), &read_target, sizeof(read_target)) ==
            LIBHACK_OK);
    EXPECT(read_target == target);
    EXPECT(libhack_write_pointer_chain(
                handle, (libhack_address_t)(uintptr_t)&first_pointer, offsets,
                arraySize(offsets), &replacement_target, sizeof(int)) ==
            LIBHACK_OK);
    EXPECT(target == 456);

    EXPECT(libhack_resolve_pointer_chain(
                handle, (libhack_address_t)(uintptr_t)&negative_pointer,
                negative_offsets, arraySize(negative_offsets), &resolved) ==
            LIBHACK_OK);
    EXPECT(resolved == (libhack_address_t)(uintptr_t)&negative_target.before);
    EXPECT(libhack_resolve_pointer_chain(handle, UINTPTR_MAX, offsets, 1,
                                         &resolved) == LIBHACK_OVERFLOW);

    EXPECT(libhack_scan_memory(handle, (libhack_address_t)(uintptr_t)bytes,
                               sizeof(bytes), pattern, mask,
                               arraySize(pattern), &matches) == LIBHACK_OK);
    EXPECT(matches.count == 1);
    EXPECT(matches.count == 0 || matches.addresses[0] ==
                                  (libhack_address_t)(uintptr_t)bytes);
    libhack_free_match_list(&matches);
    EXPECT(libhack_scan_memory(
                handle, (libhack_address_t)(uintptr_t)scan_bytes,
                sizeof(scan_bytes), pattern, mask, arraySize(pattern),
                &matches) == LIBHACK_OK);
    EXPECT(matches.count == 2);
    EXPECT(matches.count < 1 ||
           matches.addresses[0] == (libhack_address_t)(uintptr_t)scan_bytes);
    EXPECT(matches.count < 2 ||
           matches.addresses[1] ==
               (libhack_address_t)(uintptr_t)(scan_bytes + 3));
    libhack_free_match_list(&matches);
    EXPECT(libhack_scan_memory(handle, (libhack_address_t)(uintptr_t)bytes,
                               sizeof(bytes), pattern, "z?x",
                               arraySize(pattern), &matches) ==
            LIBHACK_INVALID_ARGUMENT);

    EXPECT(libhack_get_memory_regions(handle, &regions) == LIBHACK_OK);
    EXPECT(regions.count > 0);
    libhack_free_memory_regions(&regions);
    EXPECT(libhack_get_modules(handle, &modules) == LIBHACK_OK);
    EXPECT(modules.count > 0);
    if (modules.count > 0)
    {
        EXPECT(libhack_scan_module(handle, modules.items[0].name, pattern, mask,
                                   arraySize(pattern), &matches) == LIBHACK_OK);
        libhack_free_match_list(&matches);
    }
    libhack_free_modules(&modules);

    libhack_close_process(handle);
    libhack_free(handle);
}

int main(void)
{
    test_status_codes();
    test_string_lowercase();
    test_initialization();
    test_version();

#if defined(__linux__) || defined(__windows__)
    test_unified_memory_api();
#endif

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
