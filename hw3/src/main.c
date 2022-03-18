#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    size_t sz_u = 200, sz_v = 150, sz_w = 50, sz_x = 150, sz_y = 200, sz_z = 250;
    void *u = sf_malloc(sz_u);
    /* void *v = */ sf_malloc(sz_v);
    void *w = sf_malloc(sz_w);
    /* void *x = */ sf_malloc(sz_x);
    void *y = sf_malloc(sz_y);
    /* void *z = */ sf_malloc(sz_z);

    sf_free(u);
    sf_free(w);
    sf_free(y);

    sf_show_heap();

    return EXIT_SUCCESS;
}
