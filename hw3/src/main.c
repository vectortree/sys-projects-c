#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    sf_malloc(64);
    sf_malloc(64);
    sf_malloc(3200);
    //void *j = sf_malloc(64);
    //sf_free(a);
    //sf_free(j);
    //sf_show_heap();
    //sf_realloc(a, 5000);
    sf_show_heap();
    //printf("%lf\n",sf_peak_utilization());

    return EXIT_SUCCESS;
}
