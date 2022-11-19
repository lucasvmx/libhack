/**
 * @file hello.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief MessageBox dll to be injected on a process
 * @version 0.1
 * @date 2020-09-16
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <windows.h>
#include "../../process.h"

static void __stdcall ShowMessage()
{
    MessageBoxA(0, "Hello :)", "Message from hello.dll", MB_ICONINFORMATION);
}

static void SayHello()
{
    HANDLE hThread;
    DWORD threadId;

    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ShowMessage, NULL, 0, &threadId);
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, void *reserved)
{
    switch(dwReason)
    {
        case DLL_PROCESS_ATTACH:
            SayHello();
            return TRUE;

        case DLL_PROCESS_DETACH:
            return TRUE;

        case DLL_THREAD_ATTACH:
            return TRUE;

        case DLL_THREAD_DETACH:
            return TRUE;
    }

    return FALSE;
}