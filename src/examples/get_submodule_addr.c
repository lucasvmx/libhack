#include <stdio.h>
#include "../process.h"

int main()
{
	struct libhack_handle *handle = NULL;
	
	printf("initializing library...\n");

	// initializes the library
	handle = libhack_init("calculator.exe");
	if(!handle) {
		printf("failed to initialize libhack: %lu\n", GetLastError());
		return 1;
	} else {
        printf("libhack initialized\n");
    }

	// opens the process
	if(libhack_open_process(handle)) {
#ifdef __MINGW64__
		DWORD64 addr = libhack_getsubmodule_addr64(handle, "kernel32.dll");
		printf("address: 0x%llx\n", addr);
#else
		DWORD addr = libhack_getsubmodule_addr(handle, "kernel32.dll");
		printf("address: 0x%lx\n", addr);
#endif
	} else {
		printf("failed to open process\n");
	}

	return 0;
}