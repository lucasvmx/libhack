/**
 * @file dll_inject.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Example of how to use libhack to inject a dll into a target process
 * @version 0.1
 * @date 2020-09-16
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "../hack.h"
#include "../process.h"

int main()
{
    struct libhack_handle *handle = NULL;
    char full_dll_path[MAX_PATH];
    char current_directory[MAX_PATH];
    const char *dll_name = "hello.dll";
    const char *target_process = "explorer.exe";

    handle = libhack_init(target_process);
    if(!handle) {
        perror("failed to load libhack");
        return 1;
    }

    if(!libhack_open_process(handle)) {
        perror("failed to open process");
        libhack_free(handle);
        return 1;
    }

    if(!GetCurrentDirectoryA(MAX_PATH, current_directory)) {
        printf("failed to get current folder\n");
        libhack_free(handle);
        return 1;
    }

    snprintf(full_dll_path, arraySize(full_dll_path), "%s\\%s",
    current_directory,
    dll_name);

    printf("source: %s\ntarget: %s\n", full_dll_path, target_process);

    if(!libhack_inject_dll(handle, full_dll_path)) {
        perror("dll injection failed");
        libhack_free(handle);
        return FALSE;
    }

    printf("dll injected sucessfully\n");

    libhack_free(handle);

    return 0;
}