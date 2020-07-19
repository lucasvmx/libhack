/**
 * @file hack.cpp
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
#include <string.h>
#include "hack.h"

const char *libhack_getversion()
{
    return LIBHACK_VERSION;
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

BOOL libhack_open_process(struct libhack_handle *handle)
{
	if(!handle)
		return FALSE;

	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		DWORD pid = libhack_get_process_id(handle);
		if(pid)
		{
			handle->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

			return handle->hProcess == NULL ? FALSE : TRUE;
		}
	}

	/* Handle already opened */	
	SetLastError(ERROR_ALREADY_INITIALIZED);

	return FALSE;
}

DWORD libhack_get_process_id(struct libhack_handle *handle)
{
	HANDLE hSnapshot;
	PROCESSENTRY32 entry;
	DWORD pid = 0;
	size_t max_count = 0;

	/* Check if the process is already open */
	if(handle->bProcessIsOpen)
	{
		if(!handle->pid)
			return GetProcessId(handle->hProcess);
		
		return handle->pid;
	}
	
	/* Create a snapshot */
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(!hSnapshot)
	{
		libhack_debug("Failed to create snapshot\n");
		return 0;
	}

	if(!Process32First(hSnapshot, &entry)) {
		libhack_debug("Failed to initialize process list\n");
		return 0;
	}

	/* Get process exe name length */
	max_count = strlen(handle->process_name);

	do 
	{
		if(strnicmp(entry.szExeFile, handle->process_name, max_count) == 0)
		{
			pid = entry.th32ProcessID;
			break;
		}
	} while(Process32Next(hSnapshot, &entry));

	CloseHandle(hSnapshot);

	return pid;
}

int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
	int value;
	SIZE_T readed;

	/* Validate handle */
	if(!handle)
	{
		libhack_debug("Invalid handle to libhack\n");
		return 0;
	}

	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		libhack_debug("Process must be opened first\n");
		return 0;
	}

	/* Read memory at the specified address */
	ReadProcessMemory(handle->hProcess, (LPCVOID)addr, &value, sizeof(int), &readed);

	return readed ? value : 0;
}

int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value)
{
	SIZE_T written;

	/* Validate parameters */
	if(!handle)
	{
		libhack_debug("Invalid handle to libhack\n");
		return 0;
	}

	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		libhack_debug("Process must be opened first\n");
		return 0;
	}

	/* Write memory at the specified address */
	WriteProcessMemory(handle->hProcess, (void*)addr, &value, sizeof(int), &written);

	return written ? value : 0;
}
