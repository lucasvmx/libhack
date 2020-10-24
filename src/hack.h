/**
 * @file hack.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Libhack core
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
#ifdef DLL_EXPORT
#define LIBHACK_API __declspec(dllexport)
#else
#define LIBHACK_API
#endif
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
#if defined(DEBUG) || !defined(NDEBUG)
#define libhack_debug(...) fprintf(stdout, "[libhack] " __VA_ARGS__)
#else
#ifdef _MSC_VER
#define libhack_debug(...)
#else
#define libhack_debug(...) asm("nop\n\t")
#endif
#endif

#define libhack_assert_or_exit(condition, exit_code) \
	if(!condition) { \
		fprintf(stdout, "libhack assert failure on %s line %d\n", __FILE__, __LINE__); \
		exit(exit_code); \
	}

#define libhack_assert_or_warn(condition) \
	if(!condition) { \
		fprintf(stdout, "warn: assert failure on %s line %d\n", __FILE__, __LINE__); \
	}

#define libhack_assert_or_return(condition, retval) \
	if(!condition) { \
		fprintf(stdout, "warn: assert failure on %s line %d\n", __FILE__, __LINE__); \
		return retval; \
	}

/**
 * @brief Handle to libhack
 * 
 */
struct libhack_handle
{
	/**
	 * @brief Process name
	 * 
	 */
	char process_name[BUFLEN];

	/**
	 * @brief Process identifier
	 * 
	 */
	DWORD64 pid;

	/**
	 * @brief Process base address
	 * 
	 */
	DWORD64 base_addr;

	/**
	 * @brief Process handle
	 * 
	 */
	HANDLE hProcess;

	/**
	 * @brief Process module handle
	 * 
	 */
	HMODULE hModule;

	/**
	 * @brief Flag to check if process is open
	 * 
	 */
	BOOL bProcessIsOpen;
};

/**
 * @brief Gets the program version and return a string
 * 
 * @return const char* Program version
 */
LIBHACK_API const char *libhack_getversion();

/**
 * @brief Gets the commit UUID
 * 
 * @return LIBHACK_API const* UUID corresponding to commit
 */
LIBHACK_API const char *libhack_getuuid();

/**
 * @brief Gets the program build date on UTC
 * 
 * @return LIBHACK_API const* string containing build date
 */
LIBHACK_API const char *libhack_get_utc_build_date();

/**
 * @brief Initialize libhack
 * 
 * @param process_name Name of process to be accessed by library
 * @return struct libhack_handle* Handle to libhack or NULL on error
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