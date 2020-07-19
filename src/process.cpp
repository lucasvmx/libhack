/**
 * @file process.cpp
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Gerenciamento de operações com processos da memória
 * @version 0.1
 * @date 2019-08-10
 * 
 * @copyright Copyright (c) 2019 Lucas Vieira
 * 
 */

#ifdef UNICODE
#undef UNICODE
#endif

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include "process.h"

Process::Process(const char *name = "")
{
    this->name = name;
    this->base_address = 0;
    this->identifier = 0;
    this->path = "";
    this->is_open = false;
    this->hProcess = INVALID_HANDLE_VALUE;
    this->hModule = nullptr;
}

Process::Process()
{
    this->name = "";
    this->base_address = 0;
    this->identifier = 0;
    this->path = "";
    this->is_open = false;
    this->hProcess = INVALID_HANDLE_VALUE;
    this->hModule = nullptr;
}

const char *Process::GetName()
{
    return this->name.c_str();
}

unsigned long Process::GetID()
{
    tagPROCESSENTRY32 *entry = nullptr;
    HANDLE snapshot;

    // Zero identifier is the default value
    if(this->identifier > 0)
        return this->identifier;

    if(this->IsOpen())
        return GetProcessId(this->hProcess);

    // Allocate memory to store process entry data
    if(!(entry = reinterpret_cast<tagPROCESSENTRY32*>(malloc(sizeof(tagPROCESSENTRY32)))))
    {
        return 0;
    }
    
    // Take a snapshot of all running processes
    if(!(snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)))
    {
        free(entry);
        return 0;
    }
    
    if(!Process32First(snapshot, entry))
    {
        free(entry);
        return 0;
    }

    do {    
        if(strncmp(this->GetName(), entry->szExeFile, strlen(entry->szExeFile)) == 0)
        {
            this->identifier = entry->th32ProcessID;
            break;
        }

    } while(Process32Next(snapshot, entry));

    // Cleanup handle
    CloseHandle(snapshot);

    // Free up memory
    free(entry);

    return this->identifier;
}

bool Process::Open()
{
    DWORD processId;

    // Checks if the process is already open
    if(this->IsOpen()) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return false;
    }

    // Get the process ID
    processId = this->GetID();
    
    // Opens the process
    this->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if(this->hProcess == INVALID_HANDLE_VALUE) {
        this->is_open = false;
        return false;
    }

    // Update flag
    this->is_open = true;

    return true;
}

void Process::Close()
{
    if(this->IsOpen())
        CloseHandle(this->hProcess);
}

bool Process::IsOpen()
{
    return this->is_open;
}

DWORD64 Process::GetBaseAddress64()
{
    DWORD needed;
    char procName[BUFLEN];
    HMODULE module;

    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Check if we have a base address already */
    if(this->base_address)
        return base_address;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(this->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_ALL))
    {
        K32GetModuleBaseNameA(this->hProcess, module, procName, BUFLEN);

        if(strnicmp(procName, this->name.c_str(), strlen(procName)) == 0)
        {
            this->hModule = module;
            return reinterpret_cast<DWORD64>(this->hModule);
        }
    }

    return 0;
}

SIZE_T Process::ReadAddress64(DWORD64 address, void *data, size_t len)
{
    SIZE_T readed;

    if(data == nullptr)
        return 0;

    if(!ReadProcessMemory(this->hProcess, reinterpret_cast<LPCVOID>(address), data, len, &readed))
        return 0;

    return readed;
}

SIZE_T Process::WriteAddress64(DWORD64 address, void *data, size_t len)
{
    SIZE_T written;

    if(data == nullptr)
        return 0;

    if(!WriteProcessMemory(this->hProcess, reinterpret_cast<void*>(address), data, len, &written))
        return 0;

    return written;
}

int Process::ReadIntFromAddress64(DWORD64 address)
{
    int value;
    SIZE_T readed;

    ReadProcessMemory(this->hProcess, reinterpret_cast<const void*>(address), &value, sizeof(int), &readed);

    return readed ? value : 0;
}
