/**
 * @file main.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Libhack dll injection features
 * @version 0.1
 * @date 2020-07-18
 *
 * @copyright Copyright (c) 2020
 *
 */

#ifdef INJECT_DLL

#include <stdio.h>
#include <windows.h>

WINBOOL WINAPI DllMain(HINSTANCE _HDllHandle, DWORD _Reason, LPVOID _Reserved)
{
    (void)_HDllHandle;
    (void)_Reserved;

    switch (_Reason)
    {
    case DLL_PROCESS_ATTACH:
        return TRUE;
    }

    return FALSE;
}
#endif