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

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string.h>
#include "hack.h"

/**
 * @brief Contém o número da versão
 * 
 */
char version[VERSION_NUMBER_LEN];

const char *libhack_getversion()
{
	RtlSecureZeroMemory(version, sizeof(version));

	/* Print version number to variable */
	snprintf(version, arraySize(version), "%d.%d.%d", MAJOR, MINOR, PATCH);

    return (const char*)version;
}

struct libhack_handle *libhack_init(const char *process_name)
{
	struct libhack_handle *lh = NULL;
	
	/* Allocate memory to store handle data */
	lh = (struct libhack_handle*)malloc(sizeof(struct libhack_handle));
	if(!lh)
	{
		libhack_debug("Failed to allocate memory\n");
		return NULL;
	}
		
	/* Initialize memory with zeros */
	RtlSecureZeroMemory(lh->process_name, sizeof(lh->process_name));

	/* Copy process name to internal variable */
	strncpy(lh->process_name, process_name, sizeof(lh->process_name) / sizeof(lh->process_name[0]));

	lh->pid = 0;
	lh->base_addr = 0;
	lh->hProcess = NULL;
	lh->hModule = NULL;
	lh->bProcessIsOpen = FALSE;
	
	return lh;
}

void libhack_free(struct libhack_handle *lh_handle)
{
	free(lh_handle);
}
