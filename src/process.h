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

#ifdef __cplusplus
extern "C" {
#endif

#include "hack.h"

/**
 * @brief Opens the process specified when initializing library
 * 
 * @param handle Handle to libhack previously opened by libhack_init
 * @return BOOL true on success false otherwise 
 */
LIBHACK_API BOOL libhack_open_process(struct libhack_handle *handle);

/**
 * @brief Gets the process ID
 * 
 * @param handle Handle to libhack
 * @return DWORD pid of process
 */
LIBHACK_API DWORD libhack_get_process_id(struct libhack_handle *handle);

/**
 * @brief Reads a int from the specified 64bit address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be readed
 * @return int Value readed from address
 */
LIBHACK_API int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr);

/**
 * @brief Writes a int to the specified 64bit address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be written
 * @param value Value to be written on address
 * @return int Returns <= 0 on error and > 0 on success
 */
LIBHACK_API int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value);

/**
 * @brief Gets the process base address in 64bit mode
 * 
 * @param handle Handle to libhack
 * @return DWORD64 Base address of process
 */
LIBHACK_API DWORD64 libhack_get_base_addr64(struct libhack_handle *handle);

/**
 * @brief Reads a int from the specified address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be readed
 * @return int Value readed from address
 */
LIBHACK_API int libhack_read_int_from_addr(struct libhack_handle *handle, DWORD addr);

/**
 * @brief Writes a int to the specified address
 * 
 * @param handle Handle to libhack
 * @param addr Address to be written
 * @param value Value to be written on address
 * @return int Returns <= 0 on error and > 0 on success
 */
LIBHACK_API int libhack_write_int_to_addr(struct libhack_handle *handle, DWORD addr, int value);

/**
 * @brief Gets the process base address
 * 
 * @param handle Handle to libhack
 * @return DWORD Zero on error or base address of process
 */
LIBHACK_API DWORD libhack_get_base_addr(struct libhack_handle *handle);

/**
 * @brief Determines if the previously opened process still up and running
 * 
 * @param handle Handle to libhack
 * @return bool true if process is running
 */
LIBHACK_API BOOL libhack_process_is_running(struct libhack_handle *handle);

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H
