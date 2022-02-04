#include <stdlib.h>

#include "argo.h"
#include "global.h"
#include "debug.h"

int validargs(int, char **);
int equals(char *, char *);
int valid_int_string_rep(char *);
int convert_string_to_int(char *);
int only_digits(char *);
int valid_byte_range(int);

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the various options that were specified will be
 * encoded in the global variable 'global_options', where it will be
 * accessible elsewhere in the program.  For details of the required
 * encoding, see the assignment handout.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain an encoded representation
 * of the selected program options.
 */
int validargs(int argc, char **argv) {
    // TO BE IMPLEMENTED
    global_options = 0;
    if(argc <= 1)
        return -1;
    if(equals(*(argv + 1), "-h")) {
        global_options = HELP_OPTION;
        return 0;
    }
    if(argc > 4)
        return -1;
    if(equals(*(argv + 1), "-v") && argc == 2) {
        global_options = VALIDATE_OPTION;
        return 0;
    }
    if(equals(*(argv + 1), "-c")) {
        global_options = CANONICALIZE_OPTION;
        if(argc == 2)
            return 0;
        if(equals(*(argv + 2), "-p")) {
            global_options |= PRETTY_PRINT_OPTION;
            if(argc == 3) {
                global_options |= 4;
                return 0;
            }
            int indent = valid_int_string_rep(*(argv + 3));
            if(indent == -1)
                return -1;
            global_options |= indent;
            return 0;
        }
    }
    return -1;
}

int equals(char *a, char *b) {
    int i = 0;
    int j = 0;
    while(*(a + i) == *(b + i)) {
        if(*(a + i) == '\0')
            return 1;
        ++i;
        ++j;
    }
    return 0;
}

int valid_int_string_rep(char *a) {
    // This function checks if the string a is a "valid" string.
    // A "valid" string for our purposes is a string representation
    // of an int from 0x0 to 0xff.
    // It returns x if it is valid and -1 otherwise.
    int length = 0;
    while(*(a + length) != '\0')
        ++length;
    if(length > 3)
        return -1;
    if(!only_digits(a))
        return -1;
    int x = convert_string_to_int(a);
    if(!valid_byte_range(x))
        return -1;
    return x;
}

int convert_string_to_int(char *a) {
    int x = 0;
    int i = 0;
    int d = 1;
    while(*(a + i) != '\0')
        ++i;
    while(i > 0) {
        x += d * (*(a + --i) - '0');
        d *= 10;
    }
    return x;
}

int only_digits(char *a) {
    // This function returns 1 if the following conditions hold:
    // (1) The string a is not NULL,
    // (2) the string a is not empty, and
    // (3) the string a contains only numerical characters (0-9)
    //     except for the null termination character.
    if(a == NULL)
        return 0;
    if(*a == '\0')
        return 0;
    int i = 0;
    while(*(a + i) != '\0') {
        if(*(a + i) < '0' || *(a + i) > '9')
            return 0;
        ++i;
    }
    return 1;
}

int valid_byte_range(int x) {
    if(x >= 0 && x <= 0xff)
        return 1;
    return 0;
}
