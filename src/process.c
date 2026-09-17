/**
 * @file process.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Functions used to operate with processes
 * @version 0.1
 * @date 2020-09-21
 *
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#ifndef __MINGW__
#include "mingw_aliases.h"
#endif

#include "platform.h"
#include "status_codes.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __windows__
#include <windows.h>
#include <dbghelp.h>
#include <io.h>
#include <psapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#elif defined(__linux__)
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#endif

#include <assert.h>

#ifndef bool
#include <stdbool.h>
#endif

#include "logger.h"
#include "process.h"

#undef UNICODE

/**
 * @brief Checking types
 *
 */
enum CHECK_TYPES
{
    WRITE_CHECK,
    READ_CHECK
};

static libhack_status_t libhack_return_status(libhack_status_t status)
{
    libhack_set_last_error(status);
    return status;
}

#ifdef __windows__
/**
 * @brief Pointer to IsWow64Process function
 *
 */
typedef BOOL(WINAPI *pIsWow64Process)(HANDLE hProcess, PBOOL isWow64);

/**
 * @brief Checks if the specified handle can be used to specified 'type' access
 *
 * @param handle Handle to libhack
 * @param type Check to be performed
 * @return true On success
 * @return false On error
 */
static bool libhack_perform_check(struct libhack_handle *handle,
                                  enum CHECK_TYPES type)
{
    bool bCheck = true;

    if (type & WRITE_CHECK || type & READ_CHECK)
    {
        // Checks if the process can be opened for read or write
        if ((!handle) || !(handle->bProcessIsOpen))
            return false;
    }

    return bCheck;
}

/**
 * @brief Gets the number of modules loaded by specified process
 *
 * @param handle handle to libhack
 * @param b64bit TRUE if the process is 64 bit
 * @return long Number of modules loaded
 */
static long libhack_get_modules_count(struct libhack_handle *handle,
                                      short filter)
{
    HMODULE module;
    DWORD needed;
    bool status;

    status =
        K32EnumProcessModulesEx(handle->hProcess, &module, 0, &needed, filter);

    libhack_assert_or_return(status != FALSE, -1);

    return (long)needed / sizeof(HMODULE);
}

static libhack_status_t libhack_windows_record_error(DWORD error_code)
{
    libhack_set_last_native_error((int32_t)error_code);
    if (error_code == ERROR_ACCESS_DENIED)
        libhack_set_last_error(LIBHACK_ACCESS_DENIED);
    else if (error_code == ERROR_INVALID_PARAMETER)
        libhack_set_last_error(LIBHACK_INVALID_ARGUMENT);
    else if (error_code == ERROR_INVALID_ADDRESS ||
             error_code == ERROR_PARTIAL_COPY)
        libhack_set_last_error(LIBHACK_INVALID_ADDRESS);
    else if (error_code == ERROR_NOT_ENOUGH_MEMORY ||
             error_code == ERROR_OUTOFMEMORY)
        libhack_set_last_error(LIBHACK_OUT_OF_MEMORY);
    else if (error_code == ERROR_FILE_NOT_FOUND ||
             error_code == ERROR_PATH_NOT_FOUND ||
             error_code == ERROR_INVALID_HANDLE)
        libhack_set_last_error(LIBHACK_NOT_FOUND);
    else
        libhack_set_last_error(LIBHACK_NATIVE_ERROR);
    return (libhack_status_t)libhack_get_last_error();
}

libhack_status_t libhack_open_process(struct libhack_handle *handle)
{
    bool bIs64 = false;
    DWORD err;

    if (!handle)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);

    /* Check if the process is already open */
    if (!handle->bProcessIsOpen)
    {
        DWORD pid = libhack_get_process_id(handle);
        if (pid)
        {
            handle->hProcess = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                    PROCESS_VM_OPERATION,
                FALSE, pid);

            if (!handle->hProcess)
            {
                DWORD error_code = GetLastError();
                libhack_debug("Failed to open process with pid %lu: %lu", pid,
                              error_code);
                return libhack_windows_record_error(error_code);
            }

            // Setup flags
            handle->bProcessIsOpen = TRUE;

            bIs64 = libhack_is64bit_process(handle, &err);
            if (err == ERROR_SUCCESS)
            {
                libhack_debug("%s is 64-bit: %s", handle->process_name,
                              bIs64 ? "yes" : "no");
                handle->b64BitProcess = bIs64;
            }

            libhack_set_last_error(LIBHACK_OK);
            return LIBHACK_OK;
        }

        libhack_set_last_native_error(ERROR_FILE_NOT_FOUND);
        return libhack_return_status(LIBHACK_NOT_FOUND);
    }

    /* Handle already opened */
    SetLastError(ERROR_ALREADY_INITIALIZED);

    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

void libhack_close_process(struct libhack_handle *handle)
{
    if (handle == NULL)
        return;
    if (handle->hProcess != NULL)
        CloseHandle(handle->hProcess);
    handle->hProcess = NULL;
    handle->hModule = NULL;
    handle->bProcessIsOpen = FALSE;
}

DWORD libhack_get_process_id(struct libhack_handle *handle)
{
    HANDLE hSnapshot;
    PROCESSENTRY32A *entry = NULL;
    DWORD pid = 0;
    size_t max_count = 0;

    if (handle == NULL)
        return 0;

    /* A PID supplied explicitly takes precedence over name lookup. */
    if (handle->pid != 0)
        return handle->pid;

    /* Check if the process is already open */
    if (handle->bProcessIsOpen)
    {
        if (!handle->pid)
            return GetProcessId(handle->hProcess);

        return handle->pid;
    }

    /* Allocate memory */
    entry = (PROCESSENTRY32A *)malloc(sizeof(PROCESSENTRY32A));
    if (!entry)
    {
        libhack_debug("Failed to allocate memory");
        return 0;
    }
    memset(entry, 0, sizeof(*entry));
    entry->dwSize = sizeof(*entry);

    /* Create a snapshot */
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        libhack_debug("Failed to create snapshot");
        free(entry);
        return 0;
    }

    if (!Process32FirstA(hSnapshot, entry))
    {
        libhack_debug("Failed to initialize process list: %lu", GetLastError());
        CloseHandle(hSnapshot);
        free(entry);
        return 0;
    }

    /* Get process exe name length */
    max_count = strlen(handle->process_name);

    do
    {
        if (strlen(entry->szExeFile) == max_count &&
            strnicmp(entry->szExeFile, handle->process_name, max_count) == 0)
        {
            pid = entry->th32ProcessID;
            break;
        }
    } while (Process32NextA(hSnapshot, entry));

    /* Close process handle */
    CloseHandle(hSnapshot);
    free(entry);

    handle->pid = pid;

    return pid;
}

static char *libhack_windows_strdup_local(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL)
        return NULL;
    length = strlen(value) + 1;
    copy = (char *)malloc(length);
    if (copy != NULL)
        memcpy(copy, value, length);
    return copy;
}

libhack_status_t libhack_read_memory(struct libhack_handle *handle,
                                     libhack_address_t address, void *buffer,
                                     size_t size)
{
    SIZE_T transferred = 0;

    if (handle == NULL || (buffer == NULL && size != 0))
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (size == 0)
        return LIBHACK_OK;
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();
    if (!ReadProcessMemory(handle->hProcess, (const void *)address, buffer, size,
                           &transferred))
    {
        if (transferred != 0)
        {
            libhack_set_last_error(LIBHACK_PARTIAL_TRANSFER);
            return LIBHACK_PARTIAL_TRANSFER;
        }
        return libhack_windows_record_error(GetLastError());
    }
    if (transferred != size)
    {
        libhack_set_last_error(LIBHACK_PARTIAL_TRANSFER);
        return LIBHACK_PARTIAL_TRANSFER;
    }
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

libhack_status_t libhack_write_memory(struct libhack_handle *handle,
                                      libhack_address_t address,
                                      const void *buffer, size_t size)
{
    SIZE_T transferred = 0;

    if (handle == NULL || (buffer == NULL && size != 0))
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (size == 0)
        return LIBHACK_OK;
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();
    if (!WriteProcessMemory(handle->hProcess, (void *)address, buffer, size,
                            &transferred))
    {
        if (transferred != 0)
        {
            libhack_set_last_error(LIBHACK_PARTIAL_TRANSFER);
            return LIBHACK_PARTIAL_TRANSFER;
        }
        return libhack_windows_record_error(GetLastError());
    }
    if (transferred != size)
    {
        libhack_set_last_error(LIBHACK_PARTIAL_TRANSFER);
        return LIBHACK_PARTIAL_TRANSFER;
    }
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

static uint32_t libhack_windows_protection(DWORD protection)
{
    uint32_t result = 0;
    DWORD base_protection = protection & 0xff;

    if (protection & PAGE_GUARD)
        result |= LIBHACK_PROTECTION_GUARD;
    switch (base_protection)
    {
    case PAGE_READONLY:
        result |= LIBHACK_PROTECTION_READ;
        break;
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
        result |= LIBHACK_PROTECTION_READ | LIBHACK_PROTECTION_WRITE;
        break;
    case PAGE_EXECUTE:
        result |= LIBHACK_PROTECTION_EXECUTE;
        break;
    case PAGE_EXECUTE_READ:
        result |= LIBHACK_PROTECTION_EXECUTE | LIBHACK_PROTECTION_READ;
        break;
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        result |= LIBHACK_PROTECTION_EXECUTE | LIBHACK_PROTECTION_READ |
                  LIBHACK_PROTECTION_WRITE;
        break;
    default:
        break;
    }
    return result;
}

void libhack_free_memory_regions(struct libhack_memory_region_list *regions)
{
    size_t index;

    if (regions == NULL)
        return;
    for (index = 0; index < regions->count; ++index)
        free(regions->items[index].path);
    free(regions->items);
    regions->items = NULL;
    regions->count = 0;
}

libhack_status_t libhack_get_memory_regions(
    struct libhack_handle *handle, struct libhack_memory_region_list *regions)
{
    SYSTEM_INFO system_info;
    uintptr_t address;
    uintptr_t maximum_address;

    if (regions == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    regions->items = NULL;
    regions->count = 0;
    if (handle == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();

    GetSystemInfo(&system_info);
    address = (uintptr_t)system_info.lpMinimumApplicationAddress;
    maximum_address = (uintptr_t)system_info.lpMaximumApplicationAddress;
    if (!handle->b64BitProcess && maximum_address > UINT32_MAX)
        maximum_address = (uintptr_t)UINT32_MAX + 1;
    while (address < maximum_address)
    {
        MEMORY_BASIC_INFORMATION info;
        SIZE_T queried = VirtualQueryEx(handle->hProcess, (const void *)address,
                                        &info, sizeof(info));
        struct libhack_memory_region *new_items;
        struct libhack_memory_region *region;
        char mapped_path[MAX_PATH * 2];
        DWORD path_length;
        uintptr_t next_address;

        if (queried == 0)
            break;
        next_address = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next_address <= address)
            break;
        address = next_address;
        if (info.State != MEM_COMMIT)
            continue;

        new_items = (struct libhack_memory_region *)realloc(
            regions->items,
            (regions->count + 1) * sizeof(struct libhack_memory_region));
        if (new_items == NULL)
        {
            libhack_free_memory_regions(regions);
            return LIBHACK_OUT_OF_MEMORY;
        }
        regions->items = new_items;
        region = &regions->items[regions->count++];
        region->base = (libhack_address_t)info.BaseAddress;
        region->size = (size_t)info.RegionSize;
        region->protection = libhack_windows_protection(info.Protect);
        region->path = NULL;
        memset(mapped_path, 0, sizeof(mapped_path));
        path_length = GetMappedFileNameA(
            handle->hProcess, info.BaseAddress, mapped_path,
            (DWORD)arraySize(mapped_path));
        if (path_length != 0)
        {
            region->path = libhack_windows_strdup_local(mapped_path);
            if (region->path == NULL)
            {
                libhack_free_memory_regions(regions);
                return LIBHACK_OUT_OF_MEMORY;
            }
        }
    }
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

void libhack_free_modules(struct libhack_module_list *modules)
{
    size_t index;

    if (modules == NULL)
        return;
    for (index = 0; index < modules->count; ++index)
    {
        free(modules->items[index].name);
        free(modules->items[index].path);
    }
    free(modules->items);
    modules->items = NULL;
    modules->count = 0;
}

libhack_status_t libhack_get_modules(struct libhack_handle *handle,
                                     struct libhack_module_list *modules)
{
    HANDLE snapshot;
    MODULEENTRY32A entry;

    if (modules == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    modules->items = NULL;
    modules->count = 0;
    if (handle == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE |
                                            TH32CS_SNAPMODULE32,
                                        handle->pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return libhack_windows_record_error(GetLastError());

    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!Module32FirstA(snapshot, &entry))
    {
        DWORD error_code = GetLastError();
        CloseHandle(snapshot);
        return libhack_windows_record_error(error_code);
    }
    do
    {
        struct libhack_module *new_items = (struct libhack_module *)realloc(
            modules->items,
            (modules->count + 1) * sizeof(struct libhack_module));
        struct libhack_module *module;

        if (new_items == NULL)
        {
            CloseHandle(snapshot);
            libhack_free_modules(modules);
            return LIBHACK_OUT_OF_MEMORY;
        }
        modules->items = new_items;
        module = &modules->items[modules->count++];
        module->base = (libhack_address_t)entry.modBaseAddr;
        module->size = (size_t)entry.modBaseSize;
        module->name = libhack_windows_strdup_local(entry.szModule);
        module->path = libhack_windows_strdup_local(entry.szExePath);
        if (module->name == NULL || module->path == NULL)
        {
            CloseHandle(snapshot);
            libhack_free_modules(modules);
            return LIBHACK_OUT_OF_MEMORY;
        }
    } while (Module32NextA(snapshot, &entry));
    CloseHandle(snapshot);
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

static bool libhack_pattern_matches_windows(const uint8_t *buffer, size_t offset,
                                            const uint8_t *pattern,
                                            const char *mask,
                                            size_t pattern_size)
{
    size_t index;
    for (index = 0; index < pattern_size; ++index)
    {
        if (mask[index] == 'x' && buffer[offset + index] != pattern[index])
            return false;
    }
    return true;
}

static libhack_status_t libhack_windows_append_match(
    struct libhack_match_list *matches, libhack_address_t address)
{
    libhack_address_t *new_addresses = (libhack_address_t *)realloc(
        matches->addresses,
        (matches->count + 1) * sizeof(libhack_address_t));
    if (new_addresses == NULL)
        return LIBHACK_OUT_OF_MEMORY;
    matches->addresses = new_addresses;
    matches->addresses[matches->count++] = address;
    return LIBHACK_OK;
}

void libhack_free_match_list(struct libhack_match_list *matches)
{
    if (matches == NULL)
        return;
    free(matches->addresses);
    matches->addresses = NULL;
    matches->count = 0;
}

libhack_status_t libhack_scan_memory(
    struct libhack_handle *handle, libhack_address_t base, size_t size,
    const uint8_t *pattern, const char *mask, size_t pattern_size,
    struct libhack_match_list *matches)
{
    const size_t chunk_size = 1024 * 1024;
    size_t overlap;
    size_t owned_offset;
    uint8_t *buffer;
    size_t index;

    if (matches == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    matches->addresses = NULL;
    matches->count = 0;
    if (handle == NULL || pattern == NULL || mask == NULL || pattern_size == 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (strlen(mask) < pattern_size)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    for (index = 0; index < pattern_size; ++index)
    {
        if (mask[index] != 'x' && mask[index] != '?')
            return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    }
    if (size < pattern_size)
        return LIBHACK_OK;
    if (base > UINTPTR_MAX - size)
        return libhack_return_status(LIBHACK_OVERFLOW);
    overlap = pattern_size - 1;
    if (overlap > SIZE_MAX - chunk_size)
        return libhack_return_status(LIBHACK_OVERFLOW);
    buffer = (uint8_t *)malloc(chunk_size + overlap);
    if (buffer == NULL)
        return LIBHACK_OUT_OF_MEMORY;

    for (owned_offset = 0; owned_offset < size; owned_offset += chunk_size)
    {
        size_t owned_size = size - owned_offset;
        size_t read_start;
        size_t read_end;
        size_t read_size;

        if (owned_size > chunk_size)
            owned_size = chunk_size;
        read_start = owned_offset > overlap ? owned_offset - overlap : 0;
        read_end = owned_offset + owned_size;
        if (read_end < size && size - read_end > overlap)
            read_end += overlap;
        else
            read_end = size;
        read_size = read_end - read_start;
        if (libhack_read_memory(handle, base + read_start, buffer, read_size) !=
            LIBHACK_OK)
        {
            free(buffer);
            libhack_free_match_list(matches);
            return (libhack_status_t)libhack_get_last_error();
        }
        for (index = 0; index + pattern_size <= read_size; ++index)
        {
            size_t absolute_offset = read_start + index;
            if (absolute_offset < owned_offset ||
                absolute_offset >= owned_offset + owned_size)
                continue;
            if (libhack_pattern_matches_windows(buffer, index, pattern, mask,
                                                 pattern_size) &&
                libhack_windows_append_match(matches, base + absolute_offset) !=
                    LIBHACK_OK)
            {
                free(buffer);
                libhack_free_match_list(matches);
                return LIBHACK_OUT_OF_MEMORY;
            }
        }
    }
    free(buffer);
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

libhack_status_t libhack_resolve_pointer_chain(
    struct libhack_handle *handle, libhack_address_t base,
    const libhack_offset_t *offsets, size_t offset_count,
    libhack_address_t *result)
{
    libhack_address_t current;
    size_t index;

    if (handle == NULL || offsets == NULL || offset_count == 0 ||
        result == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    current = base;
    for (index = 0; index + 1 < offset_count; ++index)
    {
        libhack_address_t pointer_address;
        libhack_address_t pointer_value;
        libhack_offset_t offset = offsets[index];
        if (offset >= 0)
        {
            if (current > UINTPTR_MAX - (uintptr_t)offset)
                return libhack_return_status(LIBHACK_OVERFLOW);
            pointer_address = current + (uintptr_t)offset;
        }
        else
        {
            uintptr_t magnitude = (uintptr_t)(-(offset + 1)) + 1;
            if (current < magnitude)
                return libhack_return_status(LIBHACK_OVERFLOW);
            pointer_address = current - magnitude;
        }
        if (libhack_read_memory(handle, pointer_address, &pointer_value,
                                sizeof(pointer_value)) != LIBHACK_OK)
            return (libhack_status_t)libhack_get_last_error();
        current = pointer_value;
    }
    if (offsets[offset_count - 1] >= 0)
    {
        if (current > UINTPTR_MAX - (uintptr_t)offsets[offset_count - 1])
            return libhack_return_status(LIBHACK_OVERFLOW);
        current += (uintptr_t)offsets[offset_count - 1];
    }
    else
    {
        uintptr_t magnitude =
            (uintptr_t)(-(offsets[offset_count - 1] + 1)) + 1;
        if (current < magnitude)
            return libhack_return_status(LIBHACK_OVERFLOW);
        current -= magnitude;
    }
    *result = current;
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

libhack_status_t libhack_read_pointer_chain(
    struct libhack_handle *handle, libhack_address_t base,
    const libhack_offset_t *offsets, size_t offset_count, void *buffer,
    size_t size)
{
    libhack_address_t address;
    libhack_status_t status;
    if (buffer == NULL && size != 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    status = libhack_resolve_pointer_chain(handle, base, offsets, offset_count,
                                           &address);
    if (status != LIBHACK_OK)
        return status;
    return libhack_read_memory(handle, address, buffer, size);
}

libhack_status_t libhack_write_pointer_chain(
    struct libhack_handle *handle, libhack_address_t base,
    const libhack_offset_t *offsets, size_t offset_count, const void *buffer,
    size_t size)
{
    libhack_address_t address;
    libhack_status_t status;
    if (buffer == NULL && size != 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    status = libhack_resolve_pointer_chain(handle, base, offsets, offset_count,
                                           &address);
    if (status != LIBHACK_OK)
        return status;
    return libhack_write_memory(handle, address, buffer, size);
}

libhack_status_t libhack_scan_module(
    struct libhack_handle *handle, const char *module_name,
    const uint8_t *pattern, const char *mask, size_t pattern_size,
    struct libhack_match_list *matches)
{
    struct libhack_module_list modules = {0};
    struct libhack_memory_region_list regions = {0};
    libhack_address_t module_base = 0;
    size_t module_size = 0;
    size_t index;
    bool found_region = false;

    if (matches == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    matches->addresses = NULL;
    matches->count = 0;
    if (handle == NULL || module_name == NULL || pattern == NULL ||
        mask == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (libhack_get_modules(handle, &modules) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();
    for (index = 0; index < modules.count; ++index)
    {
        if (_stricmp(modules.items[index].name, module_name) == 0 ||
            _stricmp(modules.items[index].path, module_name) == 0)
        {
            module_base = modules.items[index].base;
            module_size = modules.items[index].size;
            break;
        }
    }
    if (module_size == 0)
    {
        libhack_free_modules(&modules);
        libhack_set_last_error(LIBHACK_NOT_FOUND);
        return LIBHACK_NOT_FOUND;
    }
    if (libhack_get_memory_regions(handle, &regions) != LIBHACK_OK)
    {
        libhack_free_modules(&modules);
        return (libhack_status_t)libhack_get_last_error();
    }
    for (index = 0; index < regions.count; ++index)
    {
        struct libhack_memory_region *region = &regions.items[index];
        libhack_address_t module_end = module_base + module_size;
        libhack_address_t region_end = region->base + region->size;
        libhack_address_t scan_base;
        size_t scan_size;
        struct libhack_match_list region_matches = {0};
        size_t match_index;
        libhack_status_t status;

        if (!(region->protection & LIBHACK_PROTECTION_READ) ||
            region_end <= module_base || region->base >= module_end)
            continue;
        scan_base = region->base > module_base ? region->base : module_base;
        scan_size = (size_t)((region_end < module_end ? region_end : module_end) -
                             scan_base);
        found_region = true;
        status = libhack_scan_memory(handle, scan_base, scan_size, pattern, mask,
                                     pattern_size, &region_matches);
        if (status != LIBHACK_OK)
        {
            libhack_free_match_list(&region_matches);
            libhack_free_memory_regions(&regions);
            libhack_free_modules(&modules);
            libhack_free_match_list(matches);
            return status;
        }
        for (match_index = 0; match_index < region_matches.count;
             ++match_index)
        {
            status = libhack_windows_append_match(
                matches, region_matches.addresses[match_index]);
            if (status != LIBHACK_OK)
            {
                libhack_free_match_list(&region_matches);
                libhack_free_memory_regions(&regions);
                libhack_free_modules(&modules);
                libhack_free_match_list(matches);
                return status;
            }
        }
        libhack_free_match_list(&region_matches);
    }
    libhack_free_memory_regions(&regions);
    libhack_free_modules(&modules);
    if (!found_region)
    {
        libhack_free_match_list(matches);
        libhack_set_last_error(LIBHACK_INVALID_ADDRESS);
        return LIBHACK_INVALID_ADDRESS;
    }
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
    int value = -1;
    SIZE_T readed;

    /* Validate handle */
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return -1;
    }

    /* Read memory at the specified address */
    if (!ReadProcessMemory(handle->hProcess, (const void *)addr, (void *)&value,
                           sizeof(int), &readed))
    {
        libhack_debug("Failed to read memory: %lu", GetLastError());
        return -1;
    }

    return readed ? value : -1;
}

int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr,
                                int value)
{
    SIZE_T written = 0;

    /* Validate parameters */
    if (!libhack_perform_check(handle, WRITE_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return -1;
    }

    /* Write memory at the specified address */
    if (!WriteProcessMemory(handle->hProcess, (void *)addr, (const void *)&value,
                            sizeof(int), &written))
    {
        libhack_debug("Failed to write memory: %lu", GetLastError());
        return -1;
    }

    return written ? value : 0;
}

DWORD64 libhack_get_base_addr64(struct libhack_handle *handle)
{
    HMODULE *modules = NULL;
    DWORD needed = 0;
    char moduleName[BUFLEN];
    long count = 0;
    unsigned short filter = LIST_MODULES_64BIT;

    /* Validate parameters */
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return 0;
    }

#ifdef __x86__
    libhack_warn("We're calling the x64 version of libhack_get_base_addr instead "
                 "of x86 version");
#elif defined(__x64__)
    if (!handle->b64BitProcess)
    {
        libhack_warn("You're trying to get the base address of a 32-bit process "
                     "using a 64-bit function");
    }
#endif

    /* Initialize memory */
    RtlSecureZeroMemory(moduleName, sizeof(moduleName));

    /* Check if we have a base address already */
    if (handle->base_addr)
    {
        return handle->base_addr;
    }

    count = libhack_get_modules_count(handle, filter);

    libhack_debug("count of 64-bit modules: %ld", count);

    // Check if previous calling failed
    libhack_assert_or_return(count > 0, 0);

    modules = (HMODULE *)malloc(sizeof(HMODULE) * count);
    if (modules == NULL)
    {
        libhack_err("We're out of memory");
        return 0;
    }

    /* Enumerate process modules */
    if (K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed,
                                filter))
    {
        for (unsigned i = 0; i < (needed / sizeof(HMODULE)); ++i)
        {
            // Get module names
            K32GetModuleBaseNameA(handle->hProcess, modules[i], moduleName,
                                  arraySize(moduleName));

            // Convert module name to lowercase
            strlwr(moduleName);

            libhack_debug("Module found: %s", moduleName);

            // Compare module name
            if (strnicmp(moduleName, handle->process_name,
                         strlen(handle->process_name)) == 0)
            {
                HMODULE modAddr = modules[i];

                // Free memory
                free(modules);

                handle->hModule = modAddr;
                return (DWORD64)(ULONG_PTR)modAddr;
            }
        }
    }
    else
    {
        libhack_err("Failed to enumerate process modules: %lu", GetLastError());
        libhack_debug("Needed/Count: %lu/%lu", needed, count);
    }

    // Free memory
    free(modules);

    libhack_debug("We failed to get process base address: %lu", GetLastError());

    return 0;
}

LIBHACK_API __int64
libhack_read_int64_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
    __int64 value = -1;
    SIZE_T readed;

    // Sanity check
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug("(%s:%d) Check failed! Either process is not opened or "
                      "handle is invalid",
                      __FILE__, __LINE__);
        return -1;
    }

    /* Read memory at the specified address */
    if (!ReadProcessMemory(handle->hProcess, (const void *)addr, (void *)&value,
                           sizeof(__int64), &readed))
    {
        libhack_debug("Failed to read memory: %lu", GetLastError());
        return -1;
    }

    return readed ? value : -1;
}

LIBHACK_API int libhack_read_int_from_addr(struct libhack_handle *handle,
                                           DWORD addr)
{
    int value = -1;
    SIZE_T readed;

    /* Validate handle */
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return -1;
    }

    /* Read memory at the specified address */
    if (!ReadProcessMemory(handle->hProcess, (const void *)addr, (void *)&value,
                           sizeof(int), &readed))
    {
        libhack_debug("Failed to read memory: %lu", GetLastError());
        return -1;
    }

    return readed ? value : -1;
}

LIBHACK_API int libhack_write_int_to_addr(struct libhack_handle *handle,
                                          DWORD addr, int value)
{
    SIZE_T written = 0;

    /* Validate parameters */
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return -1;
    }

    /* Write memory at the specified address */
    if (!WriteProcessMemory(handle->hProcess, (void *)addr, (const void *)&value,
                            sizeof(int), &written))
    {
        libhack_debug("Failed to write memory: %lu\n", GetLastError());
        return -1;
    }

    return written ? value : 0;
}

LIBHACK_API DWORD libhack_get_base_addr(struct libhack_handle *handle)
{
    HMODULE *modules = NULL;
    DWORD needed;
    char procName[BUFLEN];
    unsigned short filter = LIST_MODULES_32BIT;

    /* Validate parameters */
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return 0;
    }

    if (handle->b64BitProcess)
    {
        libhack_warn("We're using the 32-bit function to get a 64-bit address");
    }

    /* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Checks if we have a base address already */
    if (handle->base_addr)
        return handle->base_addr;

    long count = libhack_get_modules_count(handle, filter);
    libhack_assert_or_return(count > 0, 0);

    modules = (HMODULE *)malloc(sizeof(HMODULE) * count);
    if (modules == NULL)
    {
        libhack_err("We're out of memory");
        return 0;
    }

    /* Enumerate process modules */
    if (K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed,
                                filter))
    {
        for (unsigned i = 0; i < count; i++)
        {

            // Get module name
            K32GetModuleBaseNameA(handle->hProcess, modules[i], procName, BUFLEN);

            // Transform name to lowercase
            strlwr(procName);

            // Check if module is the main module
            if (strnicmp(procName, handle->process_name,
                         strlen(handle->process_name)) == 0)
            {
                DWORD modAddr = (DWORD)modules[i];
                handle->hModule = modules[i];

                // Free memory
                free(modules);

                return modAddr;
            }
        }
    }
    else
    {
        libhack_err("Failed to enumarate process modules");
    }

    // Cleanup resources
    free(modules);

    libhack_debug("Failed to get process base address: %u", GetLastError());

    return 0;
}

LIBHACK_API bool libhack_process_is_running(struct libhack_handle *handle)
{
    DWORD state;

    // Validate parameters
    if (!libhack_perform_check(handle, READ_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return FALSE;
    }

    // Try to get exit code of the process if any
    if (!GetExitCodeProcess(handle->hProcess, &state))
    {
        libhack_debug("Failed to get process exit code");
        return FALSE;
    }

    return state == STILL_ACTIVE ? TRUE : FALSE;
}

LIBHACK_API int libhack_write_string_to_addr(struct libhack_handle *handle,
                                             DWORD addr, const char *string,
                                             size_t string_len)
{
    SIZE_T written = 0;

    /* Validate parameters */
    if (!libhack_perform_check(handle, WRITE_CHECK))
    {
        libhack_debug(
            "check failed! Either process is not opened or handle is invalid");
        return -1;
    }

    if (handle->b64BitProcess)
    {
        libhack_warn("You're trying to write a string into a x64 process by using "
                     "a 32 bit address");
    }

    /* Write memory at the specified address */
    if (!WriteProcessMemory(handle->hProcess, (void *)addr, string, string_len,
                            &written))
    {
        libhack_debug("Failed to write memory: %lu\n", GetLastError());
        return -1;
    }

    return written ? (int)written : 0;
}

int libhack_write_string_to_addr64(struct libhack_handle *handle, DWORD64 addr,
                                   const char *string, size_t string_len)
{
    SIZE_T written = 0;

    /* Validate parameters */
    if (!libhack_perform_check(handle, WRITE_CHECK))
    {
        libhack_debug(
            "Check failed! Either process is not opened or handle is invalid");
        return -1;
    }

#ifdef __x86__
    if (handle->b64BitProcess)
    {
        libhack_warn("Please, use the 32-bit version of this function: "
                     "libhack_write_string_to_addr()");
    }
#endif

    /* Write memory at the specified address */
    if (!WriteProcessMemory(handle->hProcess, (void *)addr, string, string_len,
                            &written))
    {
        libhack_debug("Failed to write memory: %lu\n", GetLastError());
        return -1;
    }

    return written ? (int)written : 0;
}

LIBHACK_API bool libhack_inject_dll(struct libhack_handle *handle,
                                    const char *dll_path)
{
    void *pDllPath = NULL;
    DWORD threadId;
    HANDLE hRemoteThread = NULL;
    DWORD waitStatus;
    HANDLE hKernel32 = NULL;
    size_t dll_path_len = 0;

    libhack_assert_or_return(handle, FALSE);
    libhack_assert_or_return(dll_path, FALSE);

    if (!handle->bProcessIsOpen)
    {
        libhack_debug("You need to call libhack_open_process() before trying to "
                      "inject dll on it");
        return false;
    }

    if (!PathFileExistsA(dll_path))
    {
        libhack_debug(
            "%s could not be found. Don't forget to specify a full path to dll",
            dll_path);
        return false;
    }

    hKernel32 = LoadLibraryA("kernel32.dll");
    libhack_assert_or_return(hKernel32, FALSE);

    dll_path_len = strlen(dll_path);

    // Allocate memory to store full path of dll to be loaded into target process
    // memory
    pDllPath = VirtualAllocEx(handle->hProcess, NULL, dll_path_len,
                              MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pDllPath)
    {
        libhack_debug("Virtual alloc failed: %lu", GetLastError());
        return false;
    }

    if (!WriteProcessMemory(handle->hProcess, pDllPath, dll_path, dll_path_len,
                            NULL))
    {
        libhack_debug("Failed to write process memory: %lu", GetLastError());
        VirtualFreeEx(handle->hProcess, pDllPath, dll_path_len, MEM_RELEASE);
        return false;
    }

#ifdef __x86__
    if (handle->b64BitProcess)
    {
        libhack_warn(
            "You're trying to inject a dll into a x64 process from a 32-bit dll");
    }
#endif

    // Creates the remote thread on target process that will load library
    hRemoteThread = CreateRemoteThread(
        handle->hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA"),
        pDllPath, 0, &threadId);

    // Checks if dll injection was completed
    if (!hRemoteThread)
    {
        libhack_debug("Failed to inject dll: %lu", GetLastError());
        return false;
    }

    // Once library is loaded we can release all resources
    if (!VirtualFreeEx(handle->hProcess, pDllPath, dll_path_len, MEM_RELEASE))
    {
        libhack_debug("DLL injection was successfull but we failed to free virtual "
                      "memory: %lu",
                      GetLastError());
    }

    libhack_assert_or_warn(CloseHandle(hRemoteThread));

    // TRUE because dll was injected on target process
    return true;
}

LIBHACK_API DWORD libhack_getsubmodule_addr(struct libhack_handle *handle,
                                            const char *module_name)
{
    char basename[MAX_PATH];
    DWORD addr = 0;
    unsigned short filter = LIST_MODULES_32BIT;

    // Parameter validation
    libhack_assert_or_return(handle, 0);
    libhack_assert_or_return(handle->bProcessIsOpen, 0);

    // Check how many modules we have
    long count = libhack_get_modules_count(handle, filter);
    libhack_assert_or_return(count > 0, 0);

    if (handle->b64BitProcess)
    {
        libhack_warn("You're are getting the submodule address loaded by a x64 "
                     "process with a 32 bit function");
    }

    HMODULE *modules = (HMODULE *)malloc(sizeof(HMODULE) * count);
    DWORD needed = 0;
    libhack_assert_or_return(modules, 0);

    if (K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed,
                                filter))
    {
        for (long i = 0; i < count; i++)
        {
            if (K32GetModuleBaseNameA(handle->hProcess, modules[i], basename,
                                      arraySize(basename)))
            {
                // Convert basename to lowercase
                strlwr(basename);

                if (strnicmp(basename, module_name, strlen(module_name)) == 0)
                {
                    addr = (DWORD)modules[i];
                    break;
                }
            }
        }
    }

    free(modules);

    return addr;
}

LIBHACK_API DWORD64 libhack_getsubmodule_addr64(struct libhack_handle *handle,
                                                const char *module_name)
{
    char basename[MAX_PATH];
    DWORD64 addr = 0;
    unsigned short filter = LIST_MODULES_64BIT;

    libhack_debug("handle: %s", (handle == NULL) ? "NULL" : "NOT NULL");
    libhack_debug("process is open: %s",
                  handle->bProcessIsOpen ? "true" : "false");

    // Parameter validation
    libhack_assert_or_return(handle, 0);
    libhack_assert_or_return(handle->bProcessIsOpen, 0);

#ifdef __x86__
    if (handle->b64BitProcess)
    {
        libhack_warn("You're calling a x64 function inside a 32-bit DLL");
        libhack_warn("Please, use the x86 version: libhack_getsubmodule_addr()");
    }
#endif

    // Check how many modules we have
    long count = libhack_get_modules_count(handle, filter);
    libhack_assert_or_return(count > 0, 0);

    HMODULE *modules = (HMODULE *)malloc(sizeof(HMODULE) * count);
    DWORD needed = 0;
    libhack_assert_or_return(modules, 0);

    if (K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed,
                                filter))
    {
        for (long i = 0; i < count; i++)
        {
            if (K32GetModuleBaseNameA(handle->hProcess, modules[i], basename,
                                      sizeof(basename)))
            {
                strlwr(basename);

                if (strnicmp(basename, module_name, strlen(module_name)) == 0)
                {
                    addr = (DWORD64)modules[i];
                    break;
                }
            }
        }
    }

    free(modules);

    return addr;
}

LIBHACK_API DWORD64 libhack_getsubmodule_addr64v2(struct libhack_handle *handle,
                                                  const char *module_name)
{
    HANDLE hSnapshot;
    MODULEENTRY32 me;
    DWORD64 addr = 0;

    // Sanity check
    libhack_assert_or_return(handle && module_name, 0);
    libhack_assert_or_return(strlen(module_name) <= arraySize(me.szModule), 0);

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, handle->pid);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        libhack_err("snapshot capture failed for %ld (%s)", handle->pid,
                    handle->process_name);
        return 0;
    }

    if (!Module32First(hSnapshot, &me))
    {
        libhack_err("failed to capture first module of %d", handle->pid);
        CloseHandle(hSnapshot);
        return 0;
    }

    do
    {
        if (strnicmp(module_name, me.szModule, strlen(module_name)) == 0)
        {
            libhack_debug("address found for %s: %lx", module_name,
                          (DWORD64)&me.modBaseAddr[0]);
            addr = (DWORD64)&me.modBaseAddr[0];
            break;
        }

    } while (Module32Next(hSnapshot, &me));

    // cleanup resources
    CloseHandle(hSnapshot);

    return addr;
}

static bool fIsWow64Process(HANDLE hProcess, DWORD *error)
{
    BOOL bIsWow64 = FALSE;
    pIsWow64Process fnIsWow64Process;
    HMODULE kernel32 = GetModuleHandleA("kernel32");

    libhack_assert_or_return(error, false);

    if (!kernel32)
    {
        libhack_err("Failed to load kernel32.dll");
        *error = GetLastError();
        return false;
    }

    fnIsWow64Process =
        (pIsWow64Process)GetProcAddress(kernel32, "IsWow64Process");

    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(hProcess, &bIsWow64))
        {
            // Set error
            *error = GetLastError();

            // Show debug message
            libhack_err("Failed to call IsWow64Process");
            FreeLibrary(kernel32);

            return false;
        }
    }

    // Free resources
    FreeLibrary(kernel32);

    // Set error code
    *error = ERROR_SUCCESS;

    return bIsWow64;
}

bool libhack_is64bit_process(struct libhack_handle *handle, DWORD *error)
{
    BOOL bWow64;

    // Call function
    bWow64 = fIsWow64Process(handle->hProcess, error);

    return !bWow64;
}

#elif defined(__linux__)

static char *libhack_strdup_local(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL)
        return NULL;

    length = strlen(value) + 1;
    copy = (char *)malloc(length);
    if (copy != NULL)
        memcpy(copy, value, length);
    return copy;
}

static libhack_status_t libhack_linux_status_from_errno(int error_code)
{
    switch (error_code)
    {
    case EINVAL:
        return LIBHACK_INVALID_ARGUMENT;
    case EACCES:
    case EPERM:
        return LIBHACK_ACCESS_DENIED;
    case EFAULT:
    case ENXIO:
        return LIBHACK_INVALID_ADDRESS;
    case ENOMEM:
        return LIBHACK_OUT_OF_MEMORY;
    case ENOSYS:
        return LIBHACK_UNSUPPORTED;
    case ENOENT:
    case ESRCH:
        return LIBHACK_NOT_FOUND;
    case EOVERFLOW:
        return libhack_return_status(LIBHACK_OVERFLOW);
    default:
        return LIBHACK_NATIVE_ERROR;
    }
}

static libhack_status_t libhack_linux_record_error(int error_code)
{
    libhack_set_last_native_error(error_code);
    libhack_set_last_error(libhack_linux_status_from_errno(error_code));
    return (libhack_status_t)libhack_get_last_error();
}

static uint32_t libhack_linux_protection(const char *permissions)
{
    uint32_t protection = 0;

    if (permissions == NULL)
        return protection;
    if (permissions[0] == 'r')
        protection |= LIBHACK_PROTECTION_READ;
    if (permissions[1] == 'w')
        protection |= LIBHACK_PROTECTION_WRITE;
    if (permissions[2] == 'x')
        protection |= LIBHACK_PROTECTION_EXECUTE;
    return protection;
}

libhack_status_t libhack_open_process(struct libhack_handle *handle)
{
    char proc_path[BUFLEN];
    struct stat proc_stat;

    if (handle == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);

    if (handle->pid == -1 && libhack_get_process_id(handle) == -1)
        return libhack_linux_record_error(errno != 0 ? errno : ESRCH);

    if (handle->pid <= 0 ||
        snprintf(proc_path, sizeof(proc_path), "/proc/%d", handle->pid) < 0 ||
        stat(proc_path, &proc_stat) != 0)
        return libhack_linux_record_error(errno != 0 ? errno : ESRCH);
    if (!S_ISDIR(proc_stat.st_mode))
        return libhack_linux_record_error(ENOENT);

    handle->process_is_open = true;
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

void libhack_close_process(struct libhack_handle *handle)
{
    if (handle != NULL)
        handle->process_is_open = false;
}

libhack_status_t libhack_read_memory(struct libhack_handle *handle,
                                     libhack_address_t address, void *buffer,
                                     size_t size)
{
    struct iovec local;
    struct iovec remote;
    ssize_t transferred;

    if (handle == NULL || (buffer == NULL && size != 0))
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (size == 0)
        return LIBHACK_OK;
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();

    local.iov_base = buffer;
    local.iov_len = size;
    remote.iov_base = (void *)address;
    remote.iov_len = size;
    transferred = process_vm_readv(handle->pid, &local, 1, &remote, 1, 0);
    if (transferred < 0)
        return libhack_linux_record_error(errno);
    if ((size_t)transferred != size)
    {
        libhack_set_last_error(LIBHACK_PARTIAL_TRANSFER);
        return LIBHACK_PARTIAL_TRANSFER;
    }

    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

libhack_status_t libhack_write_memory(struct libhack_handle *handle,
                                      libhack_address_t address,
                                      const void *buffer, size_t size)
{
    struct iovec local;
    struct iovec remote;
    ssize_t transferred;

    if (handle == NULL || (buffer == NULL && size != 0))
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (size == 0)
        return LIBHACK_OK;
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();

    local.iov_base = (void *)buffer;
    local.iov_len = size;
    remote.iov_base = (void *)address;
    remote.iov_len = size;
    transferred = process_vm_writev(handle->pid, &local, 1, &remote, 1, 0);
    if (transferred < 0)
        return libhack_linux_record_error(errno);
    if ((size_t)transferred != size)
    {
        libhack_set_last_error(LIBHACK_PARTIAL_TRANSFER);
        return LIBHACK_PARTIAL_TRANSFER;
    }

    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

void libhack_free_memory_regions(struct libhack_memory_region_list *regions)
{
    size_t index;

    if (regions == NULL)
        return;
    for (index = 0; index < regions->count; ++index)
        free(regions->items[index].path);
    free(regions->items);
    regions->items = NULL;
    regions->count = 0;
}

libhack_status_t libhack_get_memory_regions(
    struct libhack_handle *handle, struct libhack_memory_region_list *regions)
{
    char maps_path[BUFLEN];
    char *line = NULL;
    size_t line_capacity = 0;
    FILE *maps_file;
    struct stat maps_stat;

    if (regions == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    regions->items = NULL;
    regions->count = 0;
    if (handle == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (libhack_open_process(handle) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();

    if (snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", handle->pid) < 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    maps_file = fopen(maps_path, "r");
    if (maps_file == NULL)
        return libhack_linux_record_error(errno);
    if (stat(maps_path, &maps_stat) != 0)
    {
        int error_code = errno;
        fclose(maps_file);
        return libhack_linux_record_error(error_code);
    }

    while (getline(&line, &line_capacity, maps_file) >= 0)
    {
        unsigned long long start;
        unsigned long long end;
        char permissions[8] = {0};
        char path[BUFLEN] = {0};
        int fields;
        struct libhack_memory_region *new_items;
        struct libhack_memory_region *region;

        fields = sscanf(line, "%llx-%llx %7s %*s %*s %*s %255[^\n]",
                        &start, &end, permissions, path);
        if (fields < 3 || end < start || end - start > SIZE_MAX)
            continue;

        new_items = (struct libhack_memory_region *)realloc(
            regions->items,
            (regions->count + 1) * sizeof(struct libhack_memory_region));
        if (new_items == NULL)
        {
            free(line);
            fclose(maps_file);
            libhack_free_memory_regions(regions);
            return LIBHACK_OUT_OF_MEMORY;
        }
        regions->items = new_items;
        region = &regions->items[regions->count++];
        region->base = (libhack_address_t)start;
        region->size = (size_t)(end - start);
        region->protection = libhack_linux_protection(permissions);
        region->path = (fields >= 4 && path[0] != '\0')
                           ? libhack_strdup_local(path)
                           : NULL;
        if (fields >= 4 && path[0] != '\0' && region->path == NULL)
        {
            free(line);
            fclose(maps_file);
            libhack_free_memory_regions(regions);
            return LIBHACK_OUT_OF_MEMORY;
        }
    }

    free(line);
    fclose(maps_file);
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

void libhack_free_modules(struct libhack_module_list *modules)
{
    size_t index;

    if (modules == NULL)
        return;
    for (index = 0; index < modules->count; ++index)
    {
        free(modules->items[index].name);
        free(modules->items[index].path);
    }
    free(modules->items);
    modules->items = NULL;
    modules->count = 0;
}

libhack_status_t libhack_get_modules(struct libhack_handle *handle,
                                     struct libhack_module_list *modules)
{
    struct libhack_memory_region_list regions = {0};
    size_t region_index;

    if (modules == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    modules->items = NULL;
    modules->count = 0;
    if (handle == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (libhack_get_memory_regions(handle, &regions) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();

    for (region_index = 0; region_index < regions.count; ++region_index)
    {
        struct libhack_memory_region *region = &regions.items[region_index];
        size_t module_index;
        size_t path_length;
        struct libhack_module *module = NULL;

        if (region->path == NULL || region->path[0] != '/')
            continue;
        for (module_index = 0; module_index < modules->count; ++module_index)
        {
            if (strcmp(modules->items[module_index].path, region->path) == 0)
            {
                module = &modules->items[module_index];
                break;
            }
        }
        if (module == NULL)
        {
            struct libhack_module *new_items = (struct libhack_module *)realloc(
                modules->items,
                (modules->count + 1) * sizeof(struct libhack_module));
            if (new_items == NULL)
            {
                libhack_free_memory_regions(&regions);
                libhack_free_modules(modules);
                return LIBHACK_OUT_OF_MEMORY;
            }
            modules->items = new_items;
            module = &modules->items[modules->count++];
            memset(module, 0, sizeof(*module));
            module->path = libhack_strdup_local(region->path);
            path_length = strlen(region->path);
            while (path_length > 0 && region->path[path_length - 1] != '/')
                --path_length;
            module->name = libhack_strdup_local(region->path + path_length);
            if (module->path == NULL || module->name == NULL)
            {
                libhack_free_memory_regions(&regions);
                libhack_free_modules(modules);
                return LIBHACK_OUT_OF_MEMORY;
            }
            module->base = region->base;
            module->size = region->size;
        }
        else
        {
            libhack_address_t region_end = region->base + region->size;
            libhack_address_t module_end = module->base + module->size;
            libhack_address_t new_base =
                region->base < module->base ? region->base : module->base;
            libhack_address_t new_end =
                region_end > module_end ? region_end : module_end;
            module->base = new_base;
            module->size = (size_t)(new_end - new_base);
        }
    }

    libhack_free_memory_regions(&regions);
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

static bool libhack_pattern_matches(const uint8_t *buffer, size_t offset,
                                    const uint8_t *pattern, const char *mask,
                                    size_t pattern_size)
{
    size_t index;

    for (index = 0; index < pattern_size; ++index)
    {
        if (mask[index] == 'x' && buffer[offset + index] != pattern[index])
            return false;
    }
    return true;
}

static libhack_status_t libhack_match_list_append(
    struct libhack_match_list *matches, libhack_address_t address)
{
    libhack_address_t *new_addresses = (libhack_address_t *)realloc(
        matches->addresses,
        (matches->count + 1) * sizeof(libhack_address_t));
    if (new_addresses == NULL)
        return LIBHACK_OUT_OF_MEMORY;
    matches->addresses = new_addresses;
    matches->addresses[matches->count++] = address;
    return LIBHACK_OK;
}

libhack_status_t libhack_scan_memory(
    struct libhack_handle *handle, libhack_address_t base, size_t size,
    const uint8_t *pattern, const char *mask, size_t pattern_size,
    struct libhack_match_list *matches)
{
    const size_t chunk_size = 1024 * 1024;
    size_t overlap;
    size_t owned_offset;
    uint8_t *buffer;
    libhack_status_t status;
    size_t index;

    if (matches == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    matches->addresses = NULL;
    matches->count = 0;
    if (handle == NULL || pattern == NULL || mask == NULL || pattern_size == 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    if (strlen(mask) < pattern_size)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    for (index = 0; index < pattern_size; ++index)
    {
        if (mask[index] != 'x' && mask[index] != '?')
            return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    }
    if (size < pattern_size)
        return LIBHACK_OK;
    if (base > UINTPTR_MAX - size)
        return libhack_return_status(LIBHACK_OVERFLOW);
    overlap = pattern_size - 1;
    if (overlap > SIZE_MAX - chunk_size)
        return libhack_return_status(LIBHACK_OVERFLOW);
    buffer = (uint8_t *)malloc(chunk_size + overlap);
    if (buffer == NULL)
        return LIBHACK_OUT_OF_MEMORY;

    for (owned_offset = 0; owned_offset < size; owned_offset += chunk_size)
    {
        size_t owned_size = size - owned_offset;
        size_t read_start;
        size_t read_end;
        size_t read_size;

        if (owned_size > chunk_size)
            owned_size = chunk_size;
        read_start = owned_offset > overlap ? owned_offset - overlap : 0;
        read_end = owned_offset + owned_size;
        if (read_end < size && size - read_end > overlap)
            read_end += overlap;
        else
            read_end = size;
        read_size = read_end - read_start;
        status = libhack_read_memory(handle, base + read_start, buffer,
                                     read_size);
        if (status != LIBHACK_OK)
        {
            free(buffer);
            libhack_free_match_list(matches);
            return status;
        }
        for (index = 0; index + pattern_size <= read_size; ++index)
        {
            size_t absolute_offset = read_start + index;
            if (absolute_offset < owned_offset ||
                absolute_offset >= owned_offset + owned_size)
                continue;
            if (libhack_pattern_matches(buffer, index, pattern, mask,
                                        pattern_size) &&
                libhack_match_list_append(matches, base + absolute_offset) !=
                    LIBHACK_OK)
            {
                free(buffer);
                libhack_free_match_list(matches);
                return LIBHACK_OUT_OF_MEMORY;
            }
        }
    }

    free(buffer);
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

void libhack_free_match_list(struct libhack_match_list *matches)
{
    if (matches == NULL)
        return;
    free(matches->addresses);
    matches->addresses = NULL;
    matches->count = 0;
}

libhack_status_t libhack_scan_module(
    struct libhack_handle *handle, const char *module_name,
    const uint8_t *pattern, const char *mask, size_t pattern_size,
    struct libhack_match_list *matches)
{
    struct libhack_module_list modules = {0};
    struct libhack_memory_region_list regions = {0};
    const char *module_path = NULL;
    size_t index;
    bool found_region = false;

    if (matches == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    matches->addresses = NULL;
    matches->count = 0;
    if (handle == NULL || module_name == NULL || pattern == NULL ||
        mask == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);

    if (libhack_get_modules(handle, &modules) != LIBHACK_OK)
        return (libhack_status_t)libhack_get_last_error();
    for (index = 0; index < modules.count; ++index)
    {
        if (strcasecmp(modules.items[index].name, module_name) == 0 ||
            strcasecmp(modules.items[index].path, module_name) == 0)
        {
            module_path = modules.items[index].path;
            break;
        }
    }
    if (module_path == NULL)
    {
        libhack_free_modules(&modules);
        libhack_set_last_error(LIBHACK_NOT_FOUND);
        return LIBHACK_NOT_FOUND;
    }

    if (libhack_get_memory_regions(handle, &regions) != LIBHACK_OK)
    {
        libhack_free_modules(&modules);
        return (libhack_status_t)libhack_get_last_error();
    }
    for (index = 0; index < regions.count; ++index)
    {
        struct libhack_match_list region_matches = {0};
        struct libhack_memory_region *region = &regions.items[index];
        size_t match_index;
        libhack_status_t status;

        if (region->path == NULL || strcmp(region->path, module_path) != 0 ||
            !(region->protection & LIBHACK_PROTECTION_READ))
            continue;
        found_region = true;
        status = libhack_scan_memory(handle, region->base, region->size,
                                     pattern, mask, pattern_size,
                                     &region_matches);
        if (status != LIBHACK_OK)
        {
            libhack_free_match_list(&region_matches);
            libhack_free_memory_regions(&regions);
            libhack_free_modules(&modules);
            libhack_free_match_list(matches);
            return status;
        }
        for (match_index = 0; match_index < region_matches.count;
             ++match_index)
        {
            status = libhack_match_list_append(
                matches, region_matches.addresses[match_index]);
            if (status != LIBHACK_OK)
            {
                libhack_free_match_list(&region_matches);
                libhack_free_memory_regions(&regions);
                libhack_free_modules(&modules);
                libhack_free_match_list(matches);
                return status;
            }
        }
        libhack_free_match_list(&region_matches);
    }

    libhack_free_memory_regions(&regions);
    libhack_free_modules(&modules);
    if (!found_region)
    {
        libhack_free_match_list(matches);
        libhack_set_last_error(LIBHACK_INVALID_ADDRESS);
        return LIBHACK_INVALID_ADDRESS;
    }
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

libhack_status_t libhack_resolve_pointer_chain(
    struct libhack_handle *handle, libhack_address_t base,
    const libhack_offset_t *offsets, size_t offset_count,
    libhack_address_t *result)
{
    libhack_address_t current;
    size_t index;

    if (handle == NULL || offsets == NULL || offset_count == 0 ||
        result == NULL)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    current = base;
    for (index = 0; index + 1 < offset_count; ++index)
    {
        libhack_address_t pointer_address;
        libhack_address_t pointer_value;
        libhack_offset_t offset = offsets[index];

        if (offset >= 0)
        {
            if (current > UINTPTR_MAX - (uintptr_t)offset)
                return libhack_return_status(LIBHACK_OVERFLOW);
            pointer_address = current + (uintptr_t)offset;
        }
        else
        {
            uintptr_t magnitude = (uintptr_t)(-(offset + 1)) + 1;
            if (current < magnitude)
                return libhack_return_status(LIBHACK_OVERFLOW);
            pointer_address = current - magnitude;
        }
        if (libhack_read_memory(handle, pointer_address, &pointer_value,
                                sizeof(pointer_value)) != LIBHACK_OK)
            return (libhack_status_t)libhack_get_last_error();
        current = pointer_value;
    }

    if (offsets[offset_count - 1] >= 0)
    {
        if (current > UINTPTR_MAX - (uintptr_t)offsets[offset_count - 1])
            return libhack_return_status(LIBHACK_OVERFLOW);
        current += (uintptr_t)offsets[offset_count - 1];
    }
    else
    {
        uintptr_t magnitude = (uintptr_t)(-(offsets[offset_count - 1] + 1)) + 1;
        if (current < magnitude)
            return libhack_return_status(LIBHACK_OVERFLOW);
        current -= magnitude;
    }
    *result = current;
    libhack_set_last_error(LIBHACK_OK);
    return LIBHACK_OK;
}

libhack_status_t libhack_read_pointer_chain(
    struct libhack_handle *handle, libhack_address_t base,
    const libhack_offset_t *offsets, size_t offset_count, void *buffer,
    size_t size)
{
    libhack_address_t address;
    libhack_status_t status;

    if (buffer == NULL && size != 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    status = libhack_resolve_pointer_chain(handle, base, offsets, offset_count,
                                           &address);
    if (status != LIBHACK_OK)
        return status;
    return libhack_read_memory(handle, address, buffer, size);
}

libhack_status_t libhack_write_pointer_chain(
    struct libhack_handle *handle, libhack_address_t base,
    const libhack_offset_t *offsets, size_t offset_count, const void *buffer,
    size_t size)
{
    libhack_address_t address;
    libhack_status_t status;

    if (buffer == NULL && size != 0)
        return libhack_return_status(LIBHACK_INVALID_ARGUMENT);
    status = libhack_resolve_pointer_chain(handle, base, offsets, offset_count,
                                           &address);
    if (status != LIBHACK_OK)
        return status;
    return libhack_write_memory(handle, address, buffer, size);
}

static bool libhack_read_process_name(const char *pid_name,
                                      char *process_name,
                                      size_t process_name_size)
{
    char comm_path[BUFLEN];
    ssize_t bytes_read;
    int comm_fd;

    if (!pid_name || !process_name || process_name_size < 2)
        return LIBHACK_NOT_FOUND;

    if (snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", pid_name) < 0)
        return false;

    comm_fd = open(comm_path, O_RDONLY | O_CLOEXEC);
    if (comm_fd == -1)
        return false;

    bytes_read = read(comm_fd, process_name, process_name_size - 1);
    close(comm_fd);

    if (bytes_read <= 0)
        return false;

    process_name[bytes_read] = '\0';
    process_name[strcspn(process_name, "\r\n")] = '\0';

    return process_name[0] != '\0';
}

pid_t libhack_get_process_id(struct libhack_handle *handle)
{
    DIR *proc_dir;
    struct dirent *entry;
    char process_name[BUFLEN];

    // Sanity check
    libhack_assert_or_return(handle != NULL, -1);

    if (handle->pid == -1)
    {
        proc_dir = opendir("/proc");
        if (!proc_dir)
        {
            libhack_err("Failed to list processes: %d", errno);
            return -1;
        }

        // Iterates through process list
        libhack_notice("reading process list ...");

        while ((entry = readdir(proc_dir)) != NULL)
        {
            char *pid_end;
            long pid_value;

            if (!isdigit((unsigned char)entry->d_name[0]))
                continue;

            errno = 0;
            pid_end = NULL;
            pid_value = strtol(entry->d_name, &pid_end, 10);
            if (errno != 0 || pid_end == entry->d_name || *pid_end != '\0' ||
                pid_value <= 0 || pid_value > INT_MAX)
                continue;

            if (libhack_read_process_name(entry->d_name, process_name,
                                          sizeof(process_name)) &&
                strcmp(process_name, handle->process_name) == 0)
            {
                handle->pid = (pid_t)pid_value;
                libhack_notice("pid of %s: %d", process_name, (int)handle->pid);
                break;
            }
        }

        libhack_notice("cleaning up resources");

        closedir(proc_dir);
    }

    return handle->pid;
}

long libhack_read_int_from_addr(const struct libhack_handle *handle, DWORD addr,
                                int *value)
{
    return libhack_read_int_from_addr64(handle, (DWORD64)addr, value);
}

long libhack_get_base_addr(struct libhack_handle *handle)
{
    struct libhack_module_list modules = {0};
    size_t module_index;

    if (handle == NULL)
        return -1;
    if (handle->base_addr > 0)
        return handle->base_addr;
    if (libhack_get_modules(handle, &modules) == LIBHACK_OK)
    {
        for (module_index = 0; module_index < modules.count; ++module_index)
        {
            if (strcasecmp(modules.items[module_index].name,
                           handle->process_name) == 0)
            {
                handle->base_addr = (long)modules.items[module_index].base;
                libhack_free_modules(&modules);
                return handle->base_addr;
            }
        }
        libhack_free_modules(&modules);
    }

    pid_t pid;
    char maps_path[BUFLEN];
    char *line;
    char *file_content;
    size_t line_len = 1024;
    struct stat st;

    // Santity checking
    libhack_assert_or_return(handle != NULL, -1);

    // Get process ID
    if (handle->pid == -1)
    {
        pid = libhack_get_process_id(handle);
    }
    else
    {
        pid = handle->pid;
    }

    // check if we already have a base address
    if (handle->base_addr > 0)
    {
        return handle->base_addr;
    }

    line = (char *)malloc(sizeof(char) * line_len);
    libhack_assert_or_return(line != NULL, -1);

    snprintf(maps_path, arraySize(maps_path), "/proc/%d/maps", pid);

    // Get file size
    if (stat(maps_path, &st) == -1)
    {
        libhack_err("failed to stat %s: %d\n", maps_path, errno);
        free(line);
        return errno;
    }

    file_content = (char *)malloc(sizeof(char) * st.st_size);
    libhack_assert_or_return(file_content != NULL, -1)

        int fd = open(maps_path, O_RDONLY);
    if (fd == -1)
    {
        // cleanup resources
        free(line);
        free(file_content);
        return errno;
    }

    FILE *fp = fdopen(fd, "r");
    if (fp == NULL)
    {
        libhack_err("failed to open %s: %d\n", maps_path, errno);
        free(line);
        free(file_content);
        close(fd);
        return errno;
    }

    unsigned long start = 0, end = 0;
    char flags[32];
    char pathname[BUFLEN];

    memset(flags, 0, sizeof(flags));

    // read all contents of file, line by line
    while ((getline(&line, &line_len, fp)) > 0)
    {
        sscanf(line, "%lx-%lx %31s %*x %*x:%*x %*u %255s", &start, &end, flags,
               &pathname[0]);

        // The address must be readable
        if ((strstr(pathname, handle->process_name) != NULL) && flags[0] == 'r')
        {
            libhack_debug("base address: %lx", start);
            break;
        }
    }

    // close file and release resources
    fclose(fp);
    close(fd);
    free(file_content);
    free(line);

    // set base address
    handle->base_addr = (long)start;

    return start;
}

long libhack_get_base_addr64(struct libhack_handle *handle)
{
    return libhack_get_base_addr(handle);
}

long libhack_read_int_from_addr64(const struct libhack_handle *handle,
                                  DWORD64 addr, int *value)
{
    struct iovec local;
    struct iovec remote;

    // Sanity checking
    libhack_assert_or_return(handle != NULL && value != NULL, -1);

    local.iov_base = value;
    local.iov_len = sizeof(int);
    remote.iov_base = (void *)(uintptr_t)addr;
    remote.iov_len = sizeof(int);

    ssize_t readed = process_vm_readv(handle->pid, &local, 1, &remote, 1, 0);
    if (readed == -1 || readed != sizeof(int))
    {
        libhack_err("Failed to read memory at address %lx from %d: %d\n", addr,
                    handle->pid, errno);
        return errno;
    }

    return LIBHACK_OK;
}

static long libhack_read_pointer_from_addr64(const struct libhack_handle *handle,
                                             DWORD64 addr,
                                             DWORD64 *pointer_value)
{
    struct iovec local;
    struct iovec remote;
    uintptr_t value;
    ssize_t readed;

    if (handle == NULL || pointer_value == NULL)
        return -1;

    local.iov_base = &value;
    local.iov_len = sizeof(value);
    remote.iov_base = (void *)(uintptr_t)addr;
    remote.iov_len = sizeof(value);

    readed = process_vm_readv(handle->pid, &local, 1, &remote, 1, 0);
    if (readed == -1 || readed != (ssize_t)sizeof(value))
    {
        libhack_err("Failed to read pointer at address %llx from %d: %d", addr,
                    handle->pid, errno);
        return readed == -1 ? errno : EIO;
    }

    *pointer_value = (DWORD64)value;
    return LIBHACK_OK;
}

long libhack_resolve_pointer_chain64(struct libhack_handle *handle,
                                     DWORD64 base_addr,
                                     const DWORD64 *offsets,
                                     size_t offset_count,
                                     DWORD64 *target_addr)
{
    DWORD64 current_addr;
    size_t index;
    long status;

    if (handle == NULL || offsets == NULL || offset_count == 0 ||
        target_addr == NULL)
        return -1;

    if (handle->pid == -1)
    {
        errno = 0;
        if (libhack_get_process_id(handle) == -1)
            return errno != 0 ? errno : ESRCH;
    }

    current_addr = base_addr;
    for (index = 0; index + 1 < offset_count; ++index)
    {
        if (UINT64_MAX - current_addr < offsets[index])
            return EOVERFLOW;

        status = libhack_read_pointer_from_addr64(handle,
                                                   current_addr + offsets[index],
                                                   &current_addr);
        if (status != LIBHACK_OK)
            return status;
    }

    if (UINT64_MAX - current_addr < offsets[offset_count - 1])
        return EOVERFLOW;

    *target_addr = current_addr + offsets[offset_count - 1];
    return LIBHACK_OK;
}

long libhack_read_int_from_pointer_chain64(struct libhack_handle *handle,
                                           DWORD64 base_addr,
                                           const DWORD64 *offsets,
                                           size_t offset_count, int *value)
{
    DWORD64 target_addr;
    long status;

    if (value == NULL)
        return -1;

    status = libhack_resolve_pointer_chain64(handle, base_addr, offsets,
                                              offset_count, &target_addr);
    if (status != LIBHACK_OK)
        return status;

    return libhack_read_int_from_addr64(handle, target_addr, value);
}

long libhack_write_int_to_pointer_chain64(struct libhack_handle *handle,
                                          DWORD64 base_addr,
                                          const DWORD64 *offsets,
                                          size_t offset_count, int value)
{
    DWORD64 target_addr;
    long status;

    status = libhack_resolve_pointer_chain64(handle, base_addr, offsets,
                                              offset_count, &target_addr);
    if (status != LIBHACK_OK)
        return status;

    return libhack_write_int_to_addr64(handle, target_addr, value);
}

long libhack_write_int_to_addr(const struct libhack_handle *handle, DWORD addr,
                               int value)
{
    return libhack_write_int_to_addr64(handle, (DWORD64)addr, value);
}

long libhack_write_int_to_addr64(const struct libhack_handle *handle,
                                 DWORD64 addr, int value)
{
    struct iovec local;
    struct iovec remote;

    // Sanity check
    libhack_assert_or_return(handle != NULL, -1);

    local.iov_base = &value;
    local.iov_len = sizeof(value);
    remote.iov_base = (void *)(uintptr_t)addr;
    remote.iov_len = sizeof(value);

    libhack_notice("writing address %lx on %d", addr, handle->pid);
    ssize_t written = process_vm_writev(handle->pid, &local, 1, &remote, 1, 0);
    if (written == -1 || (written != sizeof(value)))
    {
        libhack_debug("Failed to write memory: %d (addr: %llx)", errno, addr);
        return errno;
    }

    return LIBHACK_OK;
}

bool libhack_process_is_running(struct libhack_handle *handle)
{
    pid_t pid;
    char image_path[16];

    memset(image_path, 0, sizeof(image_path));

    // Sanity checking
    libhack_assert_or_return(handle != NULL, false);

    // Get process ID
    pid = libhack_get_process_id(handle);
    libhack_assert_or_return(handle != NULL, false);

    // Build process path on filesystem
    snprintf(image_path, arraySize(image_path), "/proc/%d", pid);

    // Check if path is readable by current process
    DIR *d = opendir(image_path);
    if (d == NULL)
    {
        libhack_err("could not open %s", image_path);
        return false;
    }

    closedir(d);

    libhack_set_last_error(LIBHACK_OK);
    return true;
}

int libhack_write_string_to_addr(const struct libhack_handle *handle,
                                 DWORD addr, const char *string, size_t len)
{
    return libhack_write_string_to_addr64(handle, (DWORD64)addr, string, len);
}

int libhack_write_string_to_addr64(const struct libhack_handle *handle,
                                   DWORD64 addr, const char *string,
                                   size_t string_len)
{
    struct iovec local;
    struct iovec remote;

    // Sanity check
    libhack_assert_or_return(handle != NULL, -1);

    local.iov_base = (void *)string;
    local.iov_len = string_len;
    remote.iov_base = (void *)(uintptr_t)addr;
    remote.iov_len = string_len;

    libhack_notice("writing address %lx on %d", addr, handle->pid);
    ssize_t written = process_vm_writev(handle->pid, &local, 1, &remote, 1, 0);
    if (written == -1 || (written != (ssize_t)string_len))
    {
        libhack_debug("Failed to write memory: %d (addr: %llx)", errno, addr);
        return errno;
    }

    return LIBHACK_OK;
}

__int64_t libhack_read_int64_from_addr64(const struct libhack_handle *handle,
                                         DWORD64 addr)
{
    struct iovec local;
    struct iovec remote;
    __int64_t local_value;

    libhack_assert_or_return(handle, -1);

    local.iov_base = &local_value;
    local.iov_len = sizeof(__int64_t);
    remote.iov_base = (void *)(uintptr_t)addr;
    remote.iov_len = sizeof(__int64_t);

    if (process_vm_readv(handle->pid, &local, 1, &remote, 1, 0) !=
        (ssize_t)sizeof(__int64_t))
    {
        libhack_err("failed to read address %llx: %d", addr, errno);
        return errno;
    }

    return local_value;
}

#endif
