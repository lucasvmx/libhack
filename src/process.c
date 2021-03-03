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

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <assert.h>
#include <shlwapi.h>
#include <io.h>
#include <dbghelp.h>
#ifndef bool
#include <stdbool.h>
#endif
#include "process.h"
#include "platform.h"
#include "logger.h"

/**
 * @brief Checking types
 * 
 */
enum CHECK_TYPES {
	WRITE_CHECK,
	READ_CHECK
};

typedef PIMAGE_NT_HEADERS (IMAGEAPI *pImageNtHeader)(PVOID Base);

/**
 * @brief Checks if the specified handle can be used to specified 'type' access
 * 
 * @param handle Handle to libhack
 * @param type Check to be performed
 * @return true On success
 * @return false On error
 */
static bool libhack_perform_check(struct libhack_handle *handle, enum CHECK_TYPES type)
{
	bool bCheck = true;

	if(type & WRITE_CHECK || type & READ_CHECK) {
		// Checks if the process can be opened for read or write
		if((!handle) || !(handle->bProcessIsOpen))
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
static long libhack_get_modules_count(struct libhack_handle *handle, bool b64bit)
{
	HMODULE module;
	DWORD needed;
	bool status;
	DWORD filter = b64bit ? LIST_MODULES_64BIT : LIST_MODULES_32BIT;

	status = K32EnumProcessModulesEx(handle->hProcess, &module, 0, &needed, filter);

	libhack_assert_or_return(status, -1);

	return (long)needed;
}

bool libhack_open_process(struct libhack_handle *handle)
{
	bool bIs64 = false;
	DWORD err;

	if(!handle)
		return false;

	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		DWORD pid = libhack_get_process_id(handle);
		if(pid)
		{
			handle->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

			if(!handle->hProcess) {
				libhack_debug("Failed to open process with pid %lu: %lu", pid, GetLastError());
				return false;
			}

			// Setup flags
			handle->bProcessIsOpen = TRUE;
			
			bIs64 = libhack_is64bit_process(handle, &err);
			if(err == ERROR_SUCCESS) {
				libhack_debug("Settings process flags");
				handle->b64BitProcess = bIs64;
			}

			return true;
		}

		return false;
	}

	/* Handle already opened */	
	SetLastError(ERROR_ALREADY_INITIALIZED);

	return true;
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
		libhack_debug("Failed to allocate memory");
		return 0;
	}
	
	/* Create a snapshot */
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(!hSnapshot)
	{
		libhack_debug("Failed to create snapshot");
		return 0;
	}

	if(!Process32First(hSnapshot, entry)) {
		libhack_debug("Failed to initialize process list: %lu", GetLastError());
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

	handle->pid = pid;

	return pid;
}

int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
	int value = -1;
	SIZE_T readed;

	/* Validate handle */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(int), &readed)) {
		libhack_debug("Failed to read memory: %lu", GetLastError());
		return -1;
	}

	return readed ? value : -1;
}

int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, WRITE_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, (const void*)&value, sizeof(int), &written)) {
		libhack_debug("Failed to write memory: %lu", GetLastError());
		return -1;
	}

	return written ? value : 0;
}

DWORD64 libhack_get_base_addr64(struct libhack_handle *handle)
{
	HMODULE module;
    DWORD needed;
	char procName[BUFLEN];

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Check if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_64BIT))
    {
        K32GetModuleBaseNameA(handle->hProcess, module, procName, BUFLEN);

		libhack_debug("Name: %s (%s)", procName, handle->process_name);

        if(strnicmp(procName, handle->process_name, strlen(handle->process_name)) == 0)
        {
            handle->hModule = module;
			return (DWORD64)module;
        }
    }
	
	libhack_debug("We failed to get process base address: %lu", GetLastError());

	return 0;
}

LIBHACK_API int libhack_read_int_from_addr(struct libhack_handle *handle, DWORD addr)
{
	int value = -1;
	SIZE_T readed;

	/* Validate handle */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(DWORD), &readed)) {
		libhack_debug("Failed to read memory: %lu", GetLastError());
		return -1;
	}

	return readed ? value : -1;
}

LIBHACK_API int libhack_write_int_to_addr(struct libhack_handle *handle, DWORD addr, int value)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, (const void*)&value, sizeof(int), &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? value : 0;
}

LIBHACK_API DWORD libhack_get_base_addr(struct libhack_handle *handle)
{
	HMODULE module;
    DWORD needed;
	char procName[BUFLEN];

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	// Check if process is a x64 process
	if(handle->b64BitProcess) {
		libhack_warn("You're trying to get the base address of a x64 process calling a x86 function");
		libhack_notice("Please, call libhack_get_base_addr64() and try again");
		return 0;
	}

	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Checks if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_32BIT))
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

LIBHACK_API bool libhack_process_is_running(struct libhack_handle *handle)
{
	DWORD state;

	// Validate parameters
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return FALSE;
	}

	// Try to get exit code of the process if any
	if(!GetExitCodeProcess(handle->hProcess, &state))
	{
		libhack_debug("Failed to get process exit code");
		return FALSE;
	}
	
	return state == STILL_ACTIVE ? TRUE : FALSE;
}

LIBHACK_API int libhack_write_string_to_addr(struct libhack_handle *handle, DWORD addr, const char *string, size_t string_len)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, WRITE_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid");
		return -1;
	}

	if(handle->b64BitProcess) {
		libhack_warn("You're trying to write a string into a x64 process by using a 32 bit address");
	}

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, string, string_len, &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? (int)written : 0;	
}

int libhack_write_string_to_addr64(struct libhack_handle *handle, DWORD64 addr, const char *string, size_t string_len)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, WRITE_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return -1;
	}

#ifdef __x86__
	if(handle->b64BitProcess) {
		libhack_warn("Please, use the 32-bit version of this function: libhack_write_string_to_addr()");
	}
#endif

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, string, string_len, &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? (int)written : 0;
}

LIBHACK_API bool libhack_inject_dll(struct libhack_handle *handle, const char *dll_path)
{
	void *pDllPath = NULL;
	DWORD threadId;
	HANDLE hRemoteThread = NULL;
	DWORD waitStatus;
	HANDLE hKernel32 = NULL;
	size_t dll_path_len = 0;

	libhack_assert_or_return(handle, FALSE);
	libhack_assert_or_return(dll_path, FALSE);
	
	if(!handle->bProcessIsOpen) {
		libhack_debug("You need to call libhack_open_process() before trying to inject dll on it");
		return false;
	}

	if(!PathFileExistsA(dll_path)) {
		libhack_debug("%s could not be found. Don't forget to specify a full path to dll", dll_path);
		return false;
	}

	hKernel32 = LoadLibraryA("kernel32.dll");
	libhack_assert_or_return(hKernel32, FALSE);

	dll_path_len = strlen(dll_path);

	// Allocate memory to store full path of dll to be loaded into target process memory
	pDllPath = VirtualAllocEx(handle->hProcess, NULL, dll_path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if(!pDllPath) {
		libhack_debug("Virtual alloc failed: %lu", GetLastError());
		return false;
	}

	if(!WriteProcessMemory(handle->hProcess, pDllPath, dll_path, dll_path_len, NULL)) {
		libhack_debug("Failed to write process memory: %lu", GetLastError());
		VirtualFreeEx(handle->hProcess, pDllPath, dll_path_len, MEM_RELEASE);
		return false;
	}

#ifdef __x86__
	if(handle->b64BitProcess) {
		libhack_warn("You're trying to inject a dll into a x64 process from a 32-bit dll");
	}
#endif

	// Creates the remote thread on target process that will load library
	hRemoteThread = CreateRemoteThread(handle->hProcess, NULL, 0, 
	(LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA"),
	pDllPath, 0, &threadId);

	// Checks if dll injection was completed
	if(!hRemoteThread) {
		libhack_debug("Failed to inject dll: %lu", GetLastError());
		return false;
	}

	// Once library is loaded we can release all resources
	if(!VirtualFreeEx(handle->hProcess, pDllPath, dll_path_len, MEM_RELEASE)) {
		libhack_debug("DLL injection was successfull but we failed to free virtual memory: %lu", GetLastError());
	}

	libhack_assert_or_warn(CloseHandle(hRemoteThread));

	// TRUE because dll was injected on target process
	return true;
}

LIBHACK_API DWORD libhack_getsubmodule_addr(struct libhack_handle *handle, const char *module_name)
{
	char basename[MAX_PATH];
	DWORD addr = 0;

	// Parameter validation
	libhack_assert_or_return(handle, 0);
	libhack_assert_or_return(handle->bProcessIsOpen, 0);

	// Check how many modules we have
	long count = libhack_get_modules_count(handle, FALSE);
	libhack_assert_or_return(count > 0, 0);

	if(handle->b64BitProcess) {
		libhack_warn("You're are getting the submodule address loaded by a x64 process with a 32 bit function");
	}

	HMODULE *modules = (HMODULE*)malloc(sizeof(HMODULE) * count);
	DWORD needed = 0;
	libhack_assert_or_return(modules, 0);

	if(K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed, LIST_MODULES_32BIT))
	{
		for(long i = 0; i < count; i++) {
			if(K32GetModuleBaseNameA(handle->hProcess, modules[i], basename, sizeof(basename))) {
				char *name = strlwr(basename);

				if(strncmp(name, module_name, strlen(module_name)) == 0) {
					addr = (DWORD)modules[i];
					break;
				}
			}
		}
	}

	free(modules);

	return addr;
}

LIBHACK_API DWORD64 libhack_getsubmodule_addr64(struct libhack_handle *handle, const char *module_name)
{
	char basename[MAX_PATH];
	DWORD64 addr = 0;

	// Parameter validation
	libhack_assert_or_return(handle, 0);
	libhack_assert_or_return(handle->bProcessIsOpen, 0);

#ifdef __x86__
	if(handle->b64BitProcess) {
		libhack_warn("You're call a x64 function inside a 32-bit DLL");
		libhack_warn("Please, use the x86 version: libhack_getsubmodule_addr()");
	}
#endif

	// Check how many modules we have
	long count = libhack_get_modules_count(handle, TRUE);
	libhack_assert_or_return(count > 0, 0);

	HMODULE *modules = (HMODULE*)malloc(sizeof(HMODULE) * count);
	DWORD needed = 0;
	libhack_assert_or_return(modules, 0);

	if(K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed, LIST_MODULES_64BIT))
	{
		for(long i = 0; i < count; i++) {
			if(K32GetModuleBaseNameA(handle->hProcess, modules[i], basename, sizeof(basename))) {
				char *name = strlwr(basename);

				if(strncmp(name, module_name, strlen(module_name)) == 0) {
					addr = (DWORD64)modules[i];
					break;
				}
			}
		}
	}

	free(modules);

	return addr;	
}

bool libhack_is64bit_process(struct libhack_handle *handle, DWORD *error)
{	
	const char *dllName = "dbghelp.dll";
	HMODULE dbghelp_dll;
	pImageNtHeader fImageNtHeader;
	bool bIsx64 = false;

	// Load dll
	dbghelp_dll = LoadLibraryA(dllName);
	if(dbghelp_dll == NULL) {
		libhack_err("Failed to load %s", dllName);
		return false;
	}

	fImageNtHeader = (pImageNtHeader)GetProcAddress(dbghelp_dll, "ImageNtHeader");
	if(!fImageNtHeader) {
		if(error != NULL)
			*error = ERROR_NOT_SUPPORTED;

		libhack_err("ImageNtHeader is not supported in your system!");
		return false;
	}

#ifdef __x64__
	DWORD64 baseAddr = libhack_get_base_addr64(handle);
#else
	DWORD baseAddr = libhack_get_base_addr(handle);
#endif

	IMAGE_NT_HEADERS *header = fImageNtHeader((void*)baseAddr);
	if(header == NULL) {

		if(error != NULL)
			*error = GetLastError();
		
		libhack_err("Failed to get information about exe header: %u", GetLastError());
		FreeLibrary(dbghelp_dll);

		return false;
	}

	libhack_debug("Base address of '%s': %lx", handle->process_name, baseAddr);

	// extract machine type
	unsigned short machineType = header->FileHeader.Machine;

	if(machineType == IMAGE_FILE_MACHINE_I386)
		bIsx64 = false;
	else if((machineType == IMAGE_FILE_MACHINE_AMD64) || (machineType == IMAGE_FILE_MACHINE_IA64))
		bIsx64 = true;

	// Free resources
	FreeLibrary(dbghelp_dll);

	return bIsx64;
}
