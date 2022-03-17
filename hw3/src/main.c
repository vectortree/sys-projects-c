#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    sf_malloc(3024);
    sf_malloc(9830);
    sf_malloc(20000);

    //*ptr = 3.14159265;

    sf_show_heap();
    //printf("%.10f\n", *ptr);

    //sf_free(ptr);

    return EXIT_SUCCESS;
}
