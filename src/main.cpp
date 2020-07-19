/**
 * @file main.cpp
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <windows.h>

WINBOOL WINAPI DllMain(HANDLE _HDllHandle, DWORD _Reason, LPVOID _Reserved)
{
    (void)_HDllHandle;
    (void)_Reserved;

    switch(_Reason)
    {
        case DLL_PROCESS_ATTACH:
            return TRUE;
    }

    return FALSE;
}
