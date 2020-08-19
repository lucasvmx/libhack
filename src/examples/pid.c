/**
 * @file read_addr.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Demonstração de uso para obter o pid de um processo
 * @date 2020-08-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "../hack.h"
#include "../process.h"

int main()
{
    struct libhack_handle *hack = NULL;
    DWORD pid;
    const char *process = "explorer.exe";

    /*
        Initialize library always
    */
    hack = libhack_init(process);
    if(!hack) 
    {
        printf("Failed to initialize libhack\n");
        return 0;
    }

    /*
        We get the pid of process
    */
    pid = libhack_get_process_id(hack);

    if(pid)
        printf("PID of %s is %lu\n", process, pid);
    else
        printf("We failed to get process ID: %lu\n", GetLastError());

    /*
        Cleanup resources used by library
    */
    libhack_free(hack);

    return 0;
}

