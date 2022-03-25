#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *a = sf_malloc(130);
    void *b = sf_malloc(130);
    void *c = sf_malloc(130);
    void *d = sf_malloc(130);
    void *e = sf_malloc(130);
    void *f = sf_malloc(130);
    sf_free(a);
    sf_free(b);
    sf_free(c);
    sf_free(d);
    sf_free(e);
    sf_free(f);
    //sf_malloc(64);
    //sf_malloc(3200);
    //void *j = sf_malloc(64);
    //sf_free(a);
    //sf_free(j);
    //sf_show_heap();
    //sf_realloc(a, 5000);
    sf_show_heap();
    //printf("%lf\n",sf_peak_utilization());

    return EXIT_SUCCESS;
}
