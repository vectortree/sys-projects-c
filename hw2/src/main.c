#include <stdlib.h>

extern int original_main(int argc, char * const * argv);

int main(int argc, char * const * argv) {
    return original_main(argc, argv);
}
