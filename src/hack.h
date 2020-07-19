/**
 * @file hack.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef LIBHACK_H
#define LIBHACK_H

#include <stdio.h>
#include <windows.h>

#ifndef LIBHACK_API
#define LIBHACK_API __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LIBHACK_VERSION "0.1"
#define BUFLEN 256
#define libhack_debug(...) fprintf(stdout, __VA_ARGS__)

/**
 * @brief Handle to libhack
 * 
 */
struct libhack_handle
{
	char process_name[BUFLEN];
	DWORD64 pid;
	DWORD64 base_addr;
	HANDLE hProcess;
	HMODULE hModule;
	BOOL bProcessIsOpen;
};

LIBHACK_API const char *libhack_getversion();
LIBHACK_API struct libhack_handle *libhack_init(const char *process_name);
LIBHACK_API void libhack_free(struct libhack_handle *handle);
LIBHACK_API BOOL libhack_open_process(struct libhack_handle *handle);
LIBHACK_API DWORD libhack_get_process_id(struct libhack_handle *handle);
LIBHACK_API int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr);
LIBHACK_API int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value);

#ifdef __cplusplus
}
#endif

#endif