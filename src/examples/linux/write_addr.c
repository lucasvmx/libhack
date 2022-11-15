#include <sys/types.h>
#include "../../types.h"
#include "../../init.h"
#include "../../process.h"
#include "../../status_codes.h"

int main() {

    struct libhack_handle *lh = libhack_init("warzone2100");
    DWORD64 addr = 0x5640024572a4;

    if(!lh) {
        printf("failed to load libhack\n");
        return 1;
    }

    printf("getting process id of %s\n", lh->process_name);

    int s = libhack_get_process_id(lh);
    if(s == -1) {
        printf("failed to get process id\n");
        return 1;
    }

    printf("process id of %s: %d %d\n",lh->process_name, s, lh->pid);
    printf("writing address %lx\n", addr);

    long status = libhack_write_int_to_addr64(lh, addr, 40000);
    if(status == LIBHACK_OK) {
        printf("memory written successfully\n");
    } else {
        printf("failed to write memory\n");
    }

    libhack_free(lh);
    
    return 0;
}
