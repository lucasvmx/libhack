/**
 * @file hack.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "types.h"

#if defined(__MINGW__) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

#ifdef __linux__
#endif
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "init.h"
#include "logger.h"
#include "../autorevision.h"

/**
 * @brief Contains version number
 * 
 */
static char version[VERSION_NUMBER_LEN];

LIBHACK_API const char *libhack_get_platform()
{
#if defined(__x86__)
	return "32-bit";
#elif defined(__x64__)
	return "64-bit";
#else
	return "unknown";
#endif
}

LIBHACK_API const char *libhack_getversion()
{
#ifdef __windows__
	RtlSecureZeroMemory(version, sizeof(version));
#else
	memset(version, 0, sizeof(version)/sizeof(version[0]));
#endif

	/* Print version number to variable */
	snprintf(version, arraySize(version), "%s build %d", VCS_TAG, VCS_NUM);

    return &version[0];
}

LIBHACK_API const char *libhack_getuuid()
{
	return VCS_UUID;
}

LIBHACK_API const char *libhack_get_utc_build_date()
{
	return VCS_DATE;
}

static bool libhack_check_version()
{
	const char *uuid = libhack_getuuid();

	if(strncmp(uuid, VCS_UUID, strlen(VCS_UUID)) != 0) {
		libhack_debug("Version mismatch: %s build %d != %s\n", VCS_TAG, VCS_NUM, libhack_getversion());
		libhack_debug("This version has been built on %s\n", VCS_DATE);
		return false;
	}

	return true;
}

struct libhack_handle *libhack_init(const char *process_name)
{
	struct libhack_handle *lh = NULL;

	// Checks if loaded version is correct	
	libhack_assert_or_exit(libhack_check_version(), 1);

	/* Validate parameters */
	if(!process_name)
		return NULL;

	/* Allocate memory to store handle data */
#ifdef __windows__
	lh = (struct libhack_handle*)malloc(sizeof(struct libhack_handle));
	if(!lh)
	{
		libhack_debug("Failed to allocate memory\n");
		return NULL;
	}

	// Initialize memory
	RtlSecureZeroMemory(lh, sizeof(struct libhack_handle));
#else
	lh = (struct libhack_handle*)calloc(1, sizeof(struct libhack_handle));
	if(!lh) {
		libhack_debug("Failed to allocate memory");
	}
#endif

	/* Copy process name to internal variable */
	strncpy(lh->process_name, process_name, sizeof(lh->process_name) / sizeof(lh->process_name[0]));
	
	// Transform process name to lowercase
	strlwr(lh->process_name);

	libhack_debug("Initialized libhack version %s (%s)\n", libhack_getversion(), libhack_get_platform());

	return lh;
}

void libhack_free(struct libhack_handle *lh_handle)
{
	free(lh_handle);
}
