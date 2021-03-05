/**
 * @file process.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Functions used to operate with processes
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef PROCESS_H
#define PROCESS_H

#ifdef UNICODE
#undef UNICODE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "init.h"
#include <stdbool.h>

/**
 * @brief Opens the process specified when initializing library
 * 
 * @param handle Handle to libhack previously opened by libhack_init
 * @return bool true on success false otherwise 
 */
LIBHACK_API bool libhack_open_process(struct libhack_handle *handle);

/**
 * @brief Gets the process ID
 * 
 * @param handle Handle to libhack
 * @return DWORD pid of process
 */
LIBHACK_API DWORD libhack_get_process_id(struct libhack_handle *handle);

/**
 * @brief Reads a int from the specified address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be readed
 * @return int Value readed from address or -1 on error
 */
LIBHACK_API int libhack_read_int_from_addr(struct libhack_handle *handle, DWORD addr);

/**
 * @brief Reads a int from the specified 64bit address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be readed
 * @return int Value readed from address or -1 on error
 */
LIBHACK_API int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr);

/**
 * @brief Writes a int to the specified address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be written
 * @param value Value to be written on address
 * @return int Number of bytes written
 */
LIBHACK_API int libhack_write_int_to_addr(struct libhack_handle *handle, DWORD addr, int value);

/**
 * @brief Writes a int to the specified 64bit address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be written
 * @param value Value to be written on address
 * @return int Number of bytes written
 */
LIBHACK_API int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value);

/**
 * @brief Writes a string to the specified address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be written
 * @param string String to be written on address
 * @param string_len String length
 * @return LIBHACK_API int Number of bytes written
 */
LIBHACK_API int libhack_write_string_to_addr(struct libhack_handle *handle, DWORD addr, const char *string, size_t string_len);

/**
 * @brief Writes a string to the specified address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be written
 * @param string String to be written on address
 * @param string_len String length
 * @return LIBHACK_API int Number of bytes written
 */
LIBHACK_API int libhack_write_string_to_addr64(struct libhack_handle *handle, DWORD64 addr, const char *string, size_t string_len);

/**
 * @brief Gets the process base address
 * 
 * @param handle Handle to libhack
 * @return DWORD Zero on error or base address of process
 */
LIBHACK_API DWORD libhack_get_base_addr(struct libhack_handle *handle);

/**
 * @brief Gets the process base address in 64bit mode
 * 
 * @param handle Handle to libhack
 * @return DWORD64 Base address of process
 */
LIBHACK_API DWORD64 libhack_get_base_addr64(struct libhack_handle *handle);

/**
 * @brief Determines if the previously opened process still up and running
 * 
 * @param handle Handle to libhack
 * @return bool true if process is running false otherwise
 */
LIBHACK_API bool libhack_process_is_running(struct libhack_handle *handle);

/**
 * @brief Injects a DLL into opened process
 * 
 * @param handle Handle to libhack returned by libhack_open()
 * @param dll_path Full path of dll to be injected
 * @return bool true on success false on errors
 */
LIBHACK_API bool libhack_inject_dll(struct libhack_handle *handle, const char *dll_path);

/**
 * @brief Gets the address of a module loaded by process
 * 
 * @param handle Handle to libhack
 * @param module_name Name of the submodule
 * @return LIBHACK_API DWORD the address of submodule
 */
LIBHACK_API DWORD libhack_getsubmodule_addr(struct libhack_handle *handle, const char *module_name);

/**
 * @brief Gets the address of a module loaded by process
 * 
 * @param handle Handle to libhack
 * @param module_name Name of the submodule
 * @return LIBHACK_API DWORD64 the address of submodule
 */
LIBHACK_API DWORD64 libhack_getsubmodule_addr64(struct libhack_handle *handle, const char *module_name);

/**
 * @brief Checks if a process is a x64 process
 * 
 * @param handle Process handle (it must be opened first)
 * @param errorCode Error code (will be 0 on success)
 * @return LIBHACK_API bool true if process is a x64 process 
 */
LIBHACK_API bool libhack_is64bit_process(struct libhack_handle *handle, DWORD *error);

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H
