/**
 * @file process.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Operações com processos
 * @version 0.1
 * @date 2019-08-10
 * 
 * @copyright Copyright (c) 2019 Lucas Vieira
 * 
 */

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include "process.h"

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

			if(!handle->hProcess) {
				libhack_debug("Failed to open process with pid %lu: %lu\n", pid, GetLastError());
				return FALSE;
			}

			/* Update flag */
			handle->bProcessIsOpen = TRUE;

			return TRUE;
		}

		return FALSE;
	}

	/* Handle already opened */	
	SetLastError(ERROR_ALREADY_INITIALIZED);

	return TRUE;
}

DWORD libhack_get_process_id(struct libhack_handle *handle)
{
	HANDLE hSnapshot;
	PROCESSENTRY32 *entry = NULL;
	DWORD pid = 0;
	size_t max_count = 0;

	/* Check if the process is already open */
	if(handle->bProcessIsOpen)
	{
		if(!handle->pid)
			return GetProcessId(handle->hProcess);
		
		return handle->pid;
	}
	
	/* Allocate memory */
	entry = (PROCESSENTRY32*)malloc(sizeof(PROCESSENTRY32));
	if(!entry)
	{
		libhack_debug("Failed to allocate memory\n");
		return 0;
	}
	
	/* Create a snapshot */
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(!hSnapshot)
	{
		libhack_debug("Failed to create snapshot\n");
		return 0;
	}

	if(!Process32First(hSnapshot, entry)) {
		libhack_debug("Failed to initialize process list: %lu\n", GetLastError());
		return 0;
	}

	/* Get process exe name length */
	max_count = strlen(handle->process_name);

	do 
	{
		if(strnicmp(entry->szExeFile, handle->process_name, max_count) == 0)
		{
			pid = entry->th32ProcessID;
			break;
		}
	} while(Process32Next(hSnapshot, entry));

	/* Close process handle */
	CloseHandle(hSnapshot);

	return pid;
}

int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
	int value = -1;
	SIZE_T readed;

	/* Validate handle */
	if(!handle)
	{
		libhack_debug("Invalid handle to libhack\n");
		return -1;
	}

	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		libhack_debug("Process must be opened first\n");
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(int), &readed)) {
		libhack_debug("Failed to read memory: %lu\n", GetLastError());
		return -1;
	}

	return value;
}

int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value)
{
	SIZE_T written = 0;

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
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, (const void*)&value, sizeof(int), &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return 0;
	}

	return written ? value : 0;
}

DWORD64 libhack_get_base_addr64(struct libhack_handle *handle)
{
	HMODULE module;
    DWORD needed;
	char procName[BUFLEN];

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
	
	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Check if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_ALL))
    {
        K32GetModuleBaseNameA(handle->hProcess, module, procName, BUFLEN);

        if(strnicmp(procName, handle->process_name, strlen(handle->process_name)) == 0)
        {
            handle->hModule = module;
			return (DWORD64)module;
        }
    }
	
	return 0;
}

LIBHACK_API int libhack_read_int_from_addr(struct libhack_handle *handle, DWORD addr)
{
	int value = -1;
	SIZE_T readed;

	/* Validate handle */
	if(!handle)
	{
		libhack_debug("Invalid handle to libhack\n");
		return -1;
	}

	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		libhack_debug("Process must be opened first\n");
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(DWORD), &readed)) {
		libhack_debug("Failed to read memory: %lu\n", GetLastError());
		return -1;
	}

	return value;
}

LIBHACK_API int libhack_write_int_to_addr(struct libhack_handle *handle, DWORD addr, int value)
{
	SIZE_T written = 0;

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
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, (const void*)&value, sizeof(int), &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return 0;
	}

	return written ? value : 0;
}

LIBHACK_API DWORD libhack_get_base_addr(struct libhack_handle *handle)
{
	HMODULE module;
    DWORD needed;
	char procName[BUFLEN];

	/* Validate parameters */
	if(!handle)
	{
		libhack_debug("Invalid handle to libhack\n");
		return 0;
	}

	/* Checks if the process is open */
	if(!handle->bProcessIsOpen)
	{
		libhack_debug("Process must be opened first\n");
		return 0;
	}
	
	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Checks if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_ALL))
    {
        K32GetModuleBaseNameA(handle->hProcess, module, procName, BUFLEN);

        if(strnicmp(procName, handle->process_name, strlen(handle->process_name)) == 0)
        {
            handle->hModule = module;
			return (DWORD)module;
        }
    }
	
	return 0;   
}

LIBHACK_API BOOL libhack_process_is_running(struct libhack_handle *handle)
{
	DWORD state;

	// Validate parameters
	if(!handle)
	{
		libhack_debug("Invalid handle to libhack\n");
		return FALSE;
	}

	// Check the flag
	if(!handle->bProcessIsOpen)
		return FALSE;

	// Try to get exit code of the process if any
	if(!GetExitCodeProcess(handle->hProcess, &state))
	{
		libhack_debug("Failed to get process exit code\n");
		return FALSE;
	}
	
	return state == STILL_ACTIVE ? TRUE : FALSE;
}
