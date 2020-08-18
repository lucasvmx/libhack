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

#include "consts.h"
#include <stdio.h>
#include <windows.h>

#ifndef LIBHACK_API
#define LIBHACK_API __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Obtém o tamanho do vetor
 * 
 */
#define arraySize(x) (sizeof(x)/sizeof(x[0]))

/**
 * @brief Shows a message if the program is being debugged
 * 
 */
#ifdef DEBUG
#define libhack_debug(...) fprintf(stdout, __VA_ARGS__)
#else
#define libhack_debug(...) asm("nop\n\t");
#endif

/**
 * @brief Handle to libhack
 * 
 */
struct libhack_handle
{
	char process_name[BUFLEN]; // Process name
	DWORD64 pid; // PID of process
	DWORD64 base_addr; // Base address of process
	HANDLE hProcess; // Handle to process
	HMODULE hModule; // Module handle to process
	BOOL bProcessIsOpen; // True if process is currently open
};

/**
 * @brief Gets the program version and return a string
 * 
 * @return const char* Program version
 */
LIBHACK_API const char *libhack_getversion();

/**
 * @brief Initialize libhack
 * 
 * @param process_name Name of process to be accessed by library
 * @return struct libhack_handle* Handle to libhack
 */
LIBHACK_API struct libhack_handle *libhack_init(const char *process_name);

/**
 * @brief Cleanup resources used by libhack
 * 
 * @param handle Handle to libhack previously opened by libhack_init
 */
LIBHACK_API void libhack_free(struct libhack_handle *handle);

/**
 * @brief Cleanup resources used by libhack
 * 
 */
#define libhack_cleanup(handle) libhack_free(handle)

#ifdef __cplusplus
}
#endif

#endif