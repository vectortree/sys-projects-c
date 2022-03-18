#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *a = sf_malloc(64);
    void *b = sf_malloc(64);
    void *c = sf_malloc(64);
    void *d = sf_malloc(64);
    void *e = sf_malloc(64);
    void *f = sf_malloc(64);
    //void *g = sf_malloc(64);
    //void *h = sf_malloc(64);
    //void *i = sf_malloc(64);
    //void *j = sf_malloc(64);
    sf_free(a);
    sf_free(b);
    sf_free(c);
    sf_free(d);
    sf_free(e);
    sf_show_heap();
    sf_free(f);
    //sf_free(g);
    //sf_free(h);
    //sf_free(i);
    //sf_free(j);

    sf_show_heap();

    return EXIT_SUCCESS;
}
