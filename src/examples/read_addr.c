/**
 * @file read_addr.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Demonstração de uso da libhack para ler um valor da memória de outro programa
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
    int value;
    DWORD baseAddr;

    /*
        Initialize library always
    */
    hack = libhack_init("calculator.exe");
    if(!hack) 
    {
        printf("Failed to initialize libhack\n");
        return 0;
    }
    
    /*
        We won't be able to perform any operations in a process
        unless it is opened
    */
    if(!libhack_open_process(hack))
    {
        printf("We failed to open process\n");
        return 0;
    }

    /*
        Here we get the base address of process

        Many of values can be obtained by using a offset from
        a process base address
    */
    baseAddr = libhack_get_base_addr(hack);

    printf("Base address of %s: %#x\n", hack->process_name, baseAddr);

    /*
        Here we are reading the memory at address with a offset
        0x4c from process base address
    */
    value = libhack_read_int_from_addr(hack, baseAddr + 0x4c);

    printf("Value readed from address %#x: %d\n", baseAddr + 0x4c, value);

    /*
        Cleanup resources used by library
    */
    libhack_free(hack);

    return 0;
}
