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

#ifndef __MINGW__
#include "mingw_aliases.h"
#endif

#include "platform.h"
#include "status_codes.h"

#ifdef __windows__
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <dbghelp.h>
#include <io.h>
#elif defined(__linux__)
#define _GNU_SOURCE
#include <sys/uio.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <proc/readproc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#endif

#include <assert.h>

#ifndef bool
#include <stdbool.h>
#endif

#include "process.h"
#include "logger.h"

#undef UNICODE

/**
 * @brief Checking types
 *
 */
enum CHECK_TYPES {
	WRITE_CHECK,
	READ_CHECK
};

#ifdef __windows__
/**
 * @brief Pointer to IsWow64Process function
 *
 */
typedef bool (*pIsWow64Process)(HANDLE hProcess, bool *isWow64);

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
static long libhack_get_modules_count(struct libhack_handle *handle, short filter)
{
	HMODULE module;
	DWORD needed;
	bool status;

	status = K32EnumProcessModulesEx(handle->hProcess, &module, 0, &needed, filter);

	libhack_assert_or_return(status != FALSE, -1);

	return (long)needed / sizeof(HMODULE);
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
				libhack_debug("%s is 64-bit: %s", handle->process_name, bIs64 ? "yes" : "no");
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
	HMODULE *modules = NULL;
    DWORD needed = 0;
	char moduleName[BUFLEN];
	long count = 0;
	unsigned short filter = LIST_MODULES_64BIT;

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return 0;
	}

#ifdef __x86__
	libhack_warn("We're calling the x64 version of libhack_get_base_addr instead of x86 version");
#elif defined(__x64__)
	if(!handle->b64BitProcess) {
		libhack_warn("You're trying to get the base address of a 32-bit process using a 64-bit function");
	}
#endif

	/* Initialize memory */
    RtlSecureZeroMemory(moduleName, sizeof(moduleName));

    /* Check if we have a base address already */
    if(handle->base_addr) {
		return handle->base_addr;
	}

	count = libhack_get_modules_count(handle, filter);

	libhack_debug("count of 64-bit modules: %ld", count);

	// Check if previous calling failed
	libhack_assert_or_return(count > 0, 0);

	modules = (HMODULE*)malloc(sizeof(HMODULE) * count);
	if(modules == NULL)
	{
		libhack_err("We're out of memory");
		return 0;
	}

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed, filter))
    {
		for(unsigned i = 0; i < (needed / sizeof(HMODULE)); ++i) {
			// Get module names
			K32GetModuleBaseNameA(handle->hProcess, modules[i], moduleName, arraySize(moduleName));

			// Convert module name to lowercase
			strlwr(moduleName);

			libhack_debug("Module found: %s", moduleName);

			// Compare module name
			if(strnicmp(moduleName, handle->process_name, strlen(handle->process_name)) == 0)
			{
				DWORD64 modAddr = modules[i];

				// Free memory
				free(modules);

				handle->hModule = modAddr;
				return (DWORD64)modAddr;
			}
		}
    } else {
		libhack_err("Failed to enumerate process modules: %lu", GetLastError());
		libhack_debug("Needed/Count: %lu/%lu", needed, count);
	}

	// Free memory
	free(modules);

	libhack_debug("We failed to get process base address: %lu", GetLastError());

	return 0;
}

LIBHACK_API __int64 libhack_read_int64_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
	__int64 value = -1;
	SIZE_T readed;

	// Sanity check
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("(%s:%d) Check failed! Either process is not opened or handle is invalid", __FILE__, __LINE__);
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(__int64), &readed)) {
		libhack_debug("Failed to read memory: %lu", GetLastError());
		return -1;
	}

	return readed ? value : -1;
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
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(int), &readed)) {
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
	HMODULE *modules = NULL;
    DWORD needed;
	char procName[BUFLEN];
	unsigned short filter = LIST_MODULES_32BIT;

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("Check failed! Either process is not opened or handle is invalid");
		return 0;
	}

	if(handle->b64BitProcess) {
		libhack_warn("We're using the 32-bit function to get a 64-bit address");
	}

	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Checks if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

	long count = libhack_get_modules_count(handle, filter);
	libhack_assert_or_return(count > 0, 0);

	modules = (HMODULE*)malloc(sizeof(HMODULE) * count);
	if(modules == NULL)
	{
		libhack_err("We're out of memory");
		return 0;
	}

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed, filter))
    {
		for(unsigned i = 0; i < count; i++) {

			// Get module name
			K32GetModuleBaseNameA(handle->hProcess, modules[i], procName, BUFLEN);

			// Transform name to lowercase
			strlwr(procName);

			// Check if module is the main module
			if(strnicmp(procName, handle->process_name, strlen(handle->process_name)) == 0)
			{
				DWORD modAddr = (DWORD)modules[i];
				handle->hModule = modules[i];

				// Free memory
				free(modules);

				return modAddr;
			}
		}
    } else {
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
	unsigned short filter = LIST_MODULES_32BIT;

	// Parameter validation
	libhack_assert_or_return(handle, 0);
	libhack_assert_or_return(handle->bProcessIsOpen, 0);

	// Check how many modules we have
	long count = libhack_get_modules_count(handle, filter);
	libhack_assert_or_return(count > 0, 0);

	if(handle->b64BitProcess) {
		libhack_warn("You're are getting the submodule address loaded by a x64 process with a 32 bit function");
	}

	HMODULE *modules = (HMODULE*)malloc(sizeof(HMODULE) * count);
	DWORD needed = 0;
	libhack_assert_or_return(modules, 0);

	if(K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed, filter))
	{
		for(long i = 0; i < count; i++)
		{
			if(K32GetModuleBaseNameA(handle->hProcess, modules[i], basename, arraySize(basename)))
			{
				// Convert basename to lowercase
				strlwr(basename);

				if(strnicmp(basename, module_name, strlen(module_name)) == 0) {
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
	unsigned short filter = LIST_MODULES_64BIT;

	libhack_debug("handle: %s", (handle == NULL) ? "NULL":"NOT NULL");
	libhack_debug("process is open: %s", handle->bProcessIsOpen ? "true":"false");

	// Parameter validation
	libhack_assert_or_return(handle, 0);
	libhack_assert_or_return(handle->bProcessIsOpen, 0);

#ifdef __x86__
	if(handle->b64BitProcess) {
		libhack_warn("You're calling a x64 function inside a 32-bit DLL");
		libhack_warn("Please, use the x86 version: libhack_getsubmodule_addr()");
	}
#endif

	// Check how many modules we have
	long count = libhack_get_modules_count(handle, filter);
	libhack_assert_or_return(count > 0, 0);

	HMODULE *modules = (HMODULE*)malloc(sizeof(HMODULE) * count);
	DWORD needed = 0;
	libhack_assert_or_return(modules, 0);

	if(K32EnumProcessModulesEx(handle->hProcess, modules, count, &needed, filter))
	{
		for(long i = 0; i < count; i++)
		{
			if(K32GetModuleBaseNameA(handle->hProcess, modules[i], basename, sizeof(basename)))
			{
				strlwr(basename);

				if(strnicmp(basename, module_name, strlen(module_name)) == 0) {
					addr = (DWORD64)modules[i];
					break;
				}
			}
		}
	}

	free(modules);

	return addr;
}

LIBHACK_API DWORD64 libhack_getsubmodule_addr64v2(struct libhack_handle *handle, const char *module_name)
{
	HANDLE hSnapshot;
	MODULEENTRY32 me;
	DWORD64 addr = 0;

	// Sanity check
	libhack_assert_or_return(handle && module_name, 0);
	libhack_assert_or_return(strlen(module_name) <= arraySize(me.szModule), 0);

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, handle->pid);
	if(hSnapshot == INVALID_HANDLE_VALUE) {
		libhack_err("snapshot capture failed for %ld (%s)", handle->pid, handle->process_name);
		return 0;
	}

	if(!Module32First(hSnapshot, &me)) {
		libhack_err("failed to capture first module of %d", handle->pid);
		CloseHandle(hSnapshot);
		return 0;
	}

	do {
		if(strnicmp(module_name, me.szModule, strlen(module_name)) == 0) {
			libhack_debug("address found for %s: %lx", module_name, (DWORD64)&me.modBaseAddr[0]);
			addr = (DWORD64)&me.modBaseAddr[0];
			break;
		}

	} while(Module32Next(hSnapshot, &me));

	// cleanup resources
	CloseHandle(hSnapshot);

	return addr;
}

static bool fIsWow64Process(HANDLE hProcess, DWORD *error)
{
    bool bIsWow64 = false;
	pIsWow64Process fnIsWow64Process;
	HMODULE kernel32 = GetModuleHandleA("kernel32");

	libhack_assert_or_return(error, false);

	if(!kernel32) {
		libhack_err("Failed to load kernel32.dll");
		*error = GetLastError();
		return false;
	}

    fnIsWow64Process = (pIsWow64Process)GetProcAddress(kernel32, "IsWow64Process");

    if(NULL != fnIsWow64Process)
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

pid_t libhack_get_process_id(struct libhack_handle *handle)
{
	PROCTAB *proc = NULL;
	proc_t proc_info;

	// Sanity check
	libhack_assert_or_return(handle != NULL, -1);

	if(handle->pid == -1) {
		proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);

		if(!proc) {
			libhack_err("Failed to list processes: %d", errno);
			return -1;
		}

		// Allocates memory
		memset(&proc_info, 0, sizeof(proc_info));

		// Iterates through process list
		libhack_notice("reading process list ...");

		while (readproc(proc, &proc_info) != NULL) {
			if(strncmp(proc_info.cmd, handle->process_name, strlen(proc_info.cmd)) == 0) {
				handle->pid = proc_info.tid;
				libhack_notice("pid of %s: %hi", proc_info.cmd, handle->pid);
				libhack_debug("start code, end code, start stack: %#lx, %#lx, %#lx",
				proc_info.start_code, proc_info.end_code, proc_info.start_stack);
				break;
			}
		}

		libhack_notice("cleaning up resources");

		closeproc(proc);
	}

	return handle->pid;
}

long libhack_read_int_from_addr(struct libhack_handle *handle, DWORD addr, int *value)
{
	return libhack_read_int_from_addr64(handle, (DWORD64)addr, value);
}

long libhack_get_base_addr(struct libhack_handle *handle)
{
	pid_t pid;
	char maps_path[BUFLEN];
	char *line;
	char *file_content;
	size_t line_len = 1024;
	struct stat st;

	// Santity checking
	libhack_assert_or_return(handle != NULL, -1);

	// Get process ID
	if(handle->pid == -1) {
		pid = libhack_get_process_id(handle);
	} else {
		pid = handle->pid;
	}

	// check if we already have a base address
	if(handle->base_addr > 0) {
		return handle->base_addr;
	}

	line = (char*)malloc(sizeof(char) * line_len);
	libhack_assert_or_return(line != NULL, -1);

	snprintf(maps_path, arraySize(maps_path), "/proc/%d/maps", pid);

	// Get file size
	if(stat(maps_path, &st) == -1) {
		libhack_err("failed to stat %s: %d\n", maps_path, errno);
		free(line);
		return errno;
	}

	file_content = (char*)malloc(sizeof(char) * st.st_size);
	libhack_assert_or_return(file_content != NULL, -1);

	FILE *fd = fopen(maps_path, "r");
	if(fd == NULL) {
		libhack_err("failed to open %s: %d\n", maps_path, errno);
		free(line);
		free(file_content);
		return errno;
	}

	unsigned long start, end;
	char flags[32];
	char pathname[BUFLEN];

	memset(flags, 0, sizeof(flags));

	// read all contents of file, line by line
	while((getline(&line, &line_len, fd)) > 0) {
		sscanf(line,"%lx-%lx %31s %*x %*x:%*x %*u %255s", &start, &end, flags, &pathname[0]);

		// The address must be readable
		if((strstr(pathname, handle->process_name) != NULL) && flags[0] == 'r') {
			libhack_debug("base address: %lx", start);
			break;
		}
	}

	// close file and release resources
	fclose(fd);
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

long libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr, int *value)
{
	struct iovec local;
	struct iovec remote;

	// Sanity checking
	libhack_assert_or_return(handle != NULL && value != NULL, -1);

	local.iov_base = value;
	local.iov_len = sizeof(int);
	remote.iov_base = &addr;
	remote.iov_len = sizeof(int);

	ssize_t readed = process_vm_readv(handle->pid, &local, 1, &remote, 1, 0);
	if(readed == -1 || readed != sizeof(int)) {
		libhack_err("Failed to read memory at address %lx from %d: %d\n", addr, handle->pid, errno);
		return errno;
	}

	return LIBHACK_OK;
}

long libhack_write_int_to_addr(struct libhack_handle *handle, DWORD addr, int value)
{
	return libhack_write_int_to_addr64(handle, (DWORD64)addr, value);
}

long libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value) {
	struct iovec local;
	struct iovec remote;

	// Sanity check
	libhack_assert_or_return(handle != NULL, -1);

	local.iov_base = &value;
	local.iov_len = sizeof(value);
	remote.iov_base = (void*)addr;
	remote.iov_len = sizeof(value);

	libhack_notice("writing address %lx on %d", addr, handle->pid);
	ssize_t written = process_vm_writev(handle->pid, &local, 1, &remote, 1, 0);
	if(written == -1 || (written != sizeof(value))) {
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
	if(d == NULL) {
		libhack_err("could not open %s", image_path);
		return false;
	}

	closedir(d);

	return true;
}

int libhack_write_string_to_addr(struct libhack_handle *handle, DWORD addr, const char *string, size_t len)
{
	return libhack_write_string_to_addr64(handle, (DWORD64)addr, string, len);
}

int libhack_write_string_to_addr64(struct libhack_handle *handle, DWORD64 addr, const char *string, size_t string_len)
{
	struct iovec local;
	struct iovec remote;

	// Sanity check
	libhack_assert_or_return(handle != NULL, -1);

	local.iov_base = &string;
	local.iov_len = string_len;
	remote.iov_base = (void*)addr;
	remote.iov_len = string_len;

	libhack_notice("writing address %lx on %d", addr, handle->pid);
	ssize_t written = process_vm_writev(handle->pid, &local, 1, &remote, 1, 0);
	if(written == -1 || (written != string_len)) {
		libhack_debug("Failed to write memory: %d (addr: %llx)", errno, addr);
		return errno;
	}

	return LIBHACK_OK;
}

#endif
