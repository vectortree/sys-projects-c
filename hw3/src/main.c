#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    //sf_show_heap();
    sf_malloc(9);
    sf_show_heap();
    //*ptr = 320320320e-320;

    //printf("%f\n", *ptr);

    //sf_free(ptr);

    return EXIT_SUCCESS;
}
