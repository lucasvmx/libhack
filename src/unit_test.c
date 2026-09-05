#include <stdio.h>

#include "init.h"

int main(void)
{
    if (libhack_init(NULL) != NULL)
    {
        printf("libhack accepted an invalid process name\n");
        return 1;
    }

    struct libhack_handle *lh = libhack_init("test.exe");

    if(lh == NULL) {
        printf("libhack init failed\n");
        return 1;
    }

    printf("test passed\n");

    libhack_free(lh);
    return 0;
}
