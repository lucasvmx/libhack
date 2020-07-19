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

#include <windows.h>

#ifndef LIBHACK_API
#define LIBHACK_API __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

LIBHACK_API const char *GetLibVersion();

#ifdef __cplusplus
}
#endif

#endif