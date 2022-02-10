#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>

#include "argo.h"
#include "global.h"
#include "debug.h"

ARGO_VALUE *argo_read_value(FILE *);
void init_vars();
void next_char(ARGO_CHAR *, FILE *);
ARGO_VALUE *argo_value(FILE *);
int argo_value_helper(ARGO_CHAR *, ARGO_VALUE **, FILE *);
int eof(ARGO_CHAR);
void add_node(ARGO_VALUE **, ARGO_VALUE **);
void print_char_err(ARGO_CHAR, ARGO_CHAR, char *);
int argo_read_basic(ARGO_BASIC *, FILE *);
int argo_read_string(ARGO_STRING *, FILE *);
int argo_read_number(ARGO_NUMBER *, FILE *);
int argo_read_array(ARGO_ARRAY *, FILE *);
int argo_read_object(ARGO_OBJECT *, FILE *);
int argo_read_member(ARGO_VALUE *, FILE *);
int argo_write_value(ARGO_VALUE *, FILE *);
int argo_write(ARGO_VALUE *, FILE *);
int argo_write_basic(ARGO_BASIC *, FILE *);
int argo_write_string(ARGO_STRING *, FILE *);
int argo_write_number(ARGO_NUMBER *, FILE *);
int argo_write_array(ARGO_ARRAY *, FILE *);
int argo_write_object(ARGO_OBJECT *, FILE *);
int argo_write_member(ARGO_VALUE *, FILE *);


/**
 * @brief  Read JSON input from a specified input stream, parse it,
 * and return a data structure representing the corresponding value.
 * @details  This function reads a sequence of 8-bit bytes from
 * a specified input stream and attempts to parse it as a JSON value,
 * according to the JSON syntax standard.  If the input can be
 * successfully parsed, then a pointer to a data structure representing
 * the corresponding value is returned.  See the assignment handout for
 * information on the JSON syntax standard and how parsing can be
 * accomplished.  As discussed in the assignment handout, the returned
 * pointer must be to one of the elements of the argo_value_storage
 * array that is defined in the const.h header file.
 * In case of an error (these include failure of the input to conform
 * to the JSON standard, premature EOF on the input stream, as well as
 * other I/O errors), a one-line error message is output to standard error
 * and a NULL pointer value is returned.
 *
 * @param f  Input stream from which JSON is to be read.
 * @return  A valid pointer if the operation is completely successful,
 * NULL if there is any error.
 */


ARGO_VALUE *argo_read_value(FILE *f) {
    // TO BE IMPLEMENTED.
    init_vars();
    return argo_value(f);
}

void init_vars() {
    argo_chars_read = 0;
    argo_lines_read = 0;
    argo_next_value = 0;
    indent_level = 0;
}

void next_char(ARGO_CHAR *c, FILE *f) {
    while(argo_is_whitespace(*c)) {
        ++argo_chars_read;
        if(*c == ARGO_LF) {
            ++argo_lines_read;
            argo_chars_read = 0;
        }
        *c = fgetc(f);
    }
}

ARGO_VALUE *argo_value(FILE *f) {
    if(f == NULL) {
        fprintf(stderr, "ERROR: Null pointer argument.\n");
        return NULL;
    }
    if(argo_next_value == NUM_ARGO_VALUES) {
        fprintf(stderr, "ERROR: Out of memory. (Value capacity: %d.)\n", NUM_ARGO_VALUES);
        return NULL;
    }
    ARGO_VALUE *value = argo_value_storage + argo_next_value++;
    ARGO_CHAR c = fgetc(f);
    next_char(&c, f);
    if(c == EOF && indent_level == 0) {
        fprintf(stderr, "[%d:%d] ERROR: Blank file.\n", argo_lines_read, argo_chars_read);
        return NULL;
    }
    if(eof(c))
        return NULL;
    argo_value_helper(&c, &value, f);
    return value;
}

int argo_value_helper(ARGO_CHAR *c, ARGO_VALUE **value, FILE *f) {
    // ARGO_BASIC
    if(*c == ARGO_T || *c == ARGO_F || *c == ARGO_N) {
        ARGO_CHAR token = *c;
        ungetc(*c, f);
        if(argo_read_basic(&(*value)->content.basic, f)) {
            *value = NULL;
            return -1;
        }
        *c = fgetc(f);
        next_char(c, f);
        if((indent_level == 0 && *c == EOF) || (indent_level > 0 && (*c == ARGO_RBRACE || *c == ARGO_RBRACK || *c == ARGO_COMMA))) {
            ungetc(*c, f);
            (*value)->type = ARGO_BASIC_TYPE;
            return 0;
        }
        if(eof(*c)) {
            *value = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(argo_is_control(*c)) {
            if(token == ARGO_T)
                fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after token '%s'.\n", argo_lines_read, argo_chars_read, *c, ARGO_TRUE_TOKEN);
            else if(token == ARGO_F)
                fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after token '%s'.\n", argo_lines_read, argo_chars_read, *c, ARGO_FALSE_TOKEN);
            else
                fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after token '%s'.\n", argo_lines_read, argo_chars_read, *c, ARGO_NULL_TOKEN);
        }
        else {
            if(token == ARGO_T)
                fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after token '%s'.\n", argo_lines_read, argo_chars_read, *c, *c, ARGO_TRUE_TOKEN);
            else if(token == ARGO_F)
                fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after token '%s'.\n", argo_lines_read, argo_chars_read, *c, *c, ARGO_FALSE_TOKEN);
            else
                fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after token '%s'.\n", argo_lines_read, argo_chars_read, *c, *c, ARGO_NULL_TOKEN);
        }
        *value = NULL;
        return -1;
    }
    // ARGO_STRING
    if(*c == ARGO_QUOTE) {
        ungetc(*c, f);
        if(argo_read_string(&(*value)->content.string, f)) {
            *value = NULL;
            return -1;
        }
        // Check if next non-whitespace token is valid,
        // i.e., it is EOF for top indent level or
        // ':' after name of object member, ',', ']', '}'
        // after array element or object member value.
        *c = fgetc(f);
        next_char(c, f);
        if((indent_level == 0 && *c == EOF) || (indent_level > 0 && (*c == ARGO_COLON || *c == ARGO_COMMA || *c == ARGO_RBRACE || *c == ARGO_RBRACK))) {
            ungetc(*c, f);
            (*value)->type = ARGO_STRING_TYPE;
            return 0;
        }
        if(eof(*c)) {
            *value = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(argo_is_control(*c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after string.\n", argo_lines_read, argo_chars_read, *c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after string.\n", argo_lines_read, argo_chars_read, *c, *c);
        *value = NULL;
        return -1;
    }
    // ARGO_NUMBER
    if(argo_is_digit(*c) || *c == ARGO_MINUS) {
        ungetc(*c, f);
        if(argo_read_number(&(*value)->content.number, f)) {
            *value = NULL;
            return -1;
        }
        *c = fgetc(f);
        next_char(c, f);
        if((indent_level == 0 && *c == EOF) || (indent_level > 0 && (*c == ARGO_COMMA || *c == ARGO_RBRACE || *c == ARGO_RBRACK))) {
            ungetc(*c, f);
            (*value)->type = ARGO_NUMBER_TYPE;
            return 0;
        }
        if(eof(*c)) {
            *value = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(argo_is_control(*c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after number.\n", argo_lines_read, argo_chars_read, *c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after number.\n", argo_lines_read, argo_chars_read, *c, *c);
        *value = NULL;
        return -1;
    }
    // ARGO_ARRAY
    if(*c == ARGO_LBRACK) {
        ungetc(*c, f);
        if(argo_read_array(&(*value)->content.array, f)) {
            *value = NULL;
            return -1;
        }
        *c = fgetc(f);
        next_char(c, f);
        if((indent_level == 0 && *c == EOF) || (indent_level > 0 && (*c == ARGO_COMMA || *c == ARGO_RBRACE || *c == ARGO_RBRACK))) {
            ungetc(*c, f);
            (*value)->type = ARGO_ARRAY_TYPE;
            return 0;
        }
        if(eof(*c)) {
            *value = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(argo_is_control(*c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after array.\n", argo_lines_read, argo_chars_read, *c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after array.\n", argo_lines_read, argo_chars_read, *c, *c);
        *value = NULL;
        return -1;
    }
    // ARGO_OBJECT
    if(*c == ARGO_LBRACE) {
        ungetc(*c, f);
        if(argo_read_object(&(*value)->content.object, f)) {
            *value = NULL;
            return -1;
        }
        *c = fgetc(f);
        next_char(c, f);
        if((indent_level == 0 && *c == EOF) || (indent_level > 0 && (*c == ARGO_COMMA || *c == ARGO_RBRACE || *c == ARGO_RBRACK))) {
            ungetc(*c, f);
            (*value)->type = ARGO_OBJECT_TYPE;
            return 0;
        }
        if(eof(*c)) {
            *value = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(argo_is_control(*c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) after object.\n", argo_lines_read, argo_chars_read, *c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) after object.\n", argo_lines_read, argo_chars_read, *c, *c);
        *value = NULL;
        return -1;
    }
    ++argo_chars_read;
    if(argo_is_control(*c))
        fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) at start of value.\n", argo_lines_read, argo_chars_read, *c);
    else
        fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) at start of value.\n", argo_lines_read, argo_chars_read, *c, *c);
    *value = NULL;
    return -1;
}

int eof(ARGO_CHAR c) {
    if(c == EOF) {
        fprintf(stderr, "[%d:%d] ERROR: Premature EOF.\n", argo_lines_read, argo_chars_read);
        return 1;
    }
    return 0;
}

void print_char_err(ARGO_CHAR c1, ARGO_CHAR c2, char *c) {
    if(argo_is_control(c2))
        fprintf(stderr, "[%d:%d] ERROR: Expected '%c' (%d) but got control character (%d) for token '%s'.\n", argo_lines_read, argo_chars_read, c1, c1, c2, c);
    else
        fprintf(stderr, "[%d:%d] ERROR: Expected '%c' (%d) but got '%c' (%d) for token '%s'.\n", argo_lines_read, argo_chars_read, c1, c1, c2, c2, c);
}

int argo_read_basic_helper(ARGO_CHAR *c, FILE *f, ARGO_BASIC **b, char *token, ARGO_BASIC type) {
    for(int i = 1; *(token + i) != '\0'; ++i) {
        *c = fgetc(f);
        if(eof(*c)) {
            *b = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(*c != *(token + i)) {
            print_char_err(*(token + i), *c, token);
            *b = NULL;
            return -1;
        }
    }
    **b = type;
    return 0;
}

int argo_read_basic(ARGO_BASIC *b, FILE *f) {
    if(b == NULL || f == NULL) {
        fprintf(stderr, "ERROR: Null pointer argument.\n");
        b = NULL;
        return -1;
    }
    ARGO_CHAR c = fgetc(f);
    if(eof(c)) {
        b = NULL;
        return -1;
    }
    ++argo_chars_read;
    if(c == ARGO_N)
        return argo_read_basic_helper(&c, f, &b, ARGO_NULL_TOKEN, ARGO_NULL);
    if(c == ARGO_T)
        return argo_read_basic_helper(&c, f, &b, ARGO_TRUE_TOKEN, ARGO_TRUE);
    if(c == ARGO_F)
        return argo_read_basic_helper(&c, f, &b, ARGO_FALSE_TOKEN, ARGO_FALSE);
    if(argo_is_control(c))
        fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) at start of value.\n", argo_lines_read, argo_chars_read, c);
    else
        fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) at start of value.\n", argo_lines_read, argo_chars_read, c, c);
    b = NULL;
    return -1;
}

void print_err(ARGO_CHAR c, char *type) {
    if(argo_is_control(c))
        fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) in %s.\n", argo_lines_read, argo_chars_read, c, type);
    else
        fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) in %s.\n", argo_lines_read, argo_chars_read, c, c, type);
}

/**
 * @brief  Read JSON input from a specified input stream, attempt to
 * parse it as a JSON string literal, and return a data structure
 * representing the corresponding string.
 * @details  This function reads a sequence of 8-bit bytes from
 * a specified input stream and attempts to parse it as a JSON string
 * literal, according to the JSON syntax standard.  If the input can be
 * successfully parsed, then a pointer to a data structure representing
 * the corresponding value is returned.
 * In case of an error (these include failure of the input to conform
 * to the JSON standard, premature EOF on the input stream, as well as
 * other I/O errors), a one-line error message is output to standard error
 * and a NULL pointer value is returned.
 *
 * @param f  Input stream from which JSON is to be read.
 * @return  Zero if the operation is completely successful,
 * nonzero if there is any error.
 */
int argo_read_string(ARGO_STRING *s, FILE *f) {
    // TO BE IMPLEMENTED.
    if(s == NULL || f == NULL) {
        fprintf(stderr, "ERROR: Null pointer argument.\n");
        s = NULL;
        return -1;
    }
    ARGO_CHAR c = fgetc(f);
    if(eof(c)) {
        s = NULL;
        return -1;
    }
    ++argo_chars_read;
    if(c != ARGO_QUOTE) {
        if(argo_is_control(c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) at start of value.\n", argo_lines_read, argo_chars_read, c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) at start of value.\n", argo_lines_read, argo_chars_read, c, c);
        s = NULL;
        return -1;
    }
    c = fgetc(f);
    while(c != ARGO_QUOTE) {
        if(eof(c)) {
            s = NULL;
            return -1;
        }
        ++argo_chars_read;
        // Check if character is valid
        if(argo_is_control(c)) {
            print_err(c, "string");
            s = NULL;
            return -1;
        }
        // Handle escape characters
        if(c == ARGO_BSLASH) {
            c = fgetc(f);
            if(eof(c)) {
                s = NULL;
                return -1;
            }
            ++argo_chars_read;
            if(c == ARGO_B)
                c = ARGO_BS;
            else if(c == ARGO_F)
                c = ARGO_FF;
            else if(c == ARGO_N)
                c = ARGO_LF;
            else if(c == ARGO_R)
                c = ARGO_CR;
            else if(c == ARGO_T)
                c = ARGO_HT;
            else if(c == ARGO_U) {
                ARGO_CHAR t;
                c = 0;
                for(int i = 3; i >= 0; --i) {
                    t = fgetc(f);
                    if(eof(t)) {
                        s = NULL;
                        return -1;
                    }
                    ++argo_chars_read;
                    if(!argo_is_hex(t)) {
                        print_err(t, "Unicode escape sequence");
                        s = NULL;
                        return -1;
                    }
                    int b = 1;
                    for(int j = 0; j < i; ++j)
                        b *= 16;
                    if(t == 'a' || t == 'A')
                        c += 10 * b;
                    else if(t == 'b' || t == 'B')
                        c += 11 * b;
                    else if(t == 'c' || t == 'C')
                        c += 12 * b;
                    else if(t == 'd' || t == 'D')
                        c += 13 * b;
                    else if(t == 'e' || t == 'E')
                        c += 14 * b;
                    else if(t == 'f' || t == 'F')
                        c += 15 * b;
                    else
                        c += (t - 48) * b;
                }
            }
            else if(c != ARGO_QUOTE && c != ARGO_BSLASH && c != ARGO_FSLASH) {
                print_err(c, "escape sequence");
                s = NULL;
                return -1;
            }
        }
        // If valid, call argo_append_char()
        if(argo_append_char(s, c)) {
            s = NULL;
            return -1;
        }
        c = fgetc(f);
    }
    ++argo_chars_read;
    return 0;
}

/**
 * @brief  Read JSON input from a specified input stream, attempt to
 * parse it as a JSON number, and return a data structure representing
 * the corresponding number.
 * @details  This function reads a sequence of 8-bit bytes from
 * a specified input stream and attempts to parse it as a JSON numeric
 * literal, according to the JSON syntax standard.  If the input can be
 * successfully parsed, then a pointer to a data structure representing
 * the corresponding value is returned.  The returned value must contain
 * (1) a string consisting of the actual sequence of characters read from
 * the input stream; (2) a floating point representation of the corresponding
 * value; and (3) an integer representation of the corresponding value,
 * in case the input literal did not contain any fraction or exponent parts.
 * In case of an error (these include failure of the input to conform
 * to the JSON standard, premature EOF on the input stream, as well as
 * other I/O errors), a one-line error message is output to standard error
 * and a NULL pointer value is returned.
 *
 * @param f  Input stream from which JSON is to be read.
 * @return  Zero if the operation is completely successful,
 * nonzero if there is any error.
 */
int argo_read_number(ARGO_NUMBER *n, FILE *f) {
    // TO BE IMPLEMENTED.
    if(n == NULL || f == NULL) {
        fprintf(stderr, "ERROR: Null pointer argument.\n");
        n = NULL;
        return -1;
    }
    ARGO_CHAR c = fgetc(f);
    if(eof(c)) {
        n = NULL;
        return -1;
    }
    ++argo_chars_read;
    if(!argo_is_digit(c) && c != ARGO_MINUS) {
        if(argo_is_control(c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) at start of value.\n", argo_lines_read, argo_chars_read, c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) at start of value.\n", argo_lines_read, argo_chars_read, c, c);
        n = NULL;
        return -1;
    }
    n->valid_string = 1;
    n->valid_float = 1;
    n->valid_int = 1;
    char negative = 0;
    argo_append_char(&n->string_value, c);
    if(c == ARGO_MINUS) {
        negative = 1;
        c = fgetc(f);
        if(eof(c)) {
            n = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(!argo_is_digit(c)) {
            if(argo_is_control(c))
                fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got control character (%d) for number.\n", argo_lines_read, argo_chars_read, c);
            else
                fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got '%c' (%d) for number.\n", argo_lines_read, argo_chars_read, c, c);
            n = NULL;
            return -1;
        }
        argo_append_char(&n->string_value, c);
    }
    n->int_value = c - ARGO_DIGIT0;
    n->float_value = n->int_value;
    if(c != ARGO_DIGIT0)
        c = fgetc(f);
    else {
        c = fgetc(f);
        if(argo_is_digit(c)) {
            ++argo_chars_read;
            fprintf(stderr, "[%d:%d] ERROR: Leading zeros in number.\n", argo_lines_read, argo_chars_read);
            n = NULL;
            return -1;
        }

    }
    char int_overflow = 0;
    char float_overflow = 0;
    char int_underflow = 0;
    char float_underflow = 0;
    unsigned long long temp_llong = n->int_value;
    long double temp_ldouble = n->float_value;
    while(argo_is_digit(c)) {
        ++argo_chars_read;
        n->int_value = n->int_value * 10 + (c - ARGO_DIGIT0);
        temp_llong = temp_llong * 10 + (c - ARGO_DIGIT0);
        if(negative && temp_llong - 1 > LONG_MAX)
            int_underflow = 1;
        if(!negative && temp_llong > LONG_MAX)
            int_overflow = 1;
        n->float_value = n->float_value * 10 + (c - ARGO_DIGIT0);
        temp_ldouble = temp_ldouble * 10 + (c - ARGO_DIGIT0);
        if(negative && temp_ldouble > DBL_MAX)
            float_underflow = 1;
        if(!negative && temp_ldouble > DBL_MAX)
            float_overflow = 1;
        argo_append_char(&n->string_value, c);
        c = fgetc(f);
    }
    if(negative)
        n->int_value *= -1;
    if(c == ARGO_PERIOD) {
        ++argo_chars_read;
        argo_append_char(&n->string_value, c);
        n->valid_int = 0;
        c = fgetc(f);
        if(eof(c)) {
            n = NULL;
            return -1;
        }
        if(!argo_is_digit(c)) {
            ++argo_chars_read;
            if(argo_is_control(c))
                fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got control character (%d) for fractional part of number.\n", argo_lines_read, argo_chars_read, c);
            else
                fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got '%c' (%d) for fractional part of number.\n", argo_lines_read, argo_chars_read, c, c);
            n = NULL;
            return -1;
        }
        int count = 0;
        while(argo_is_digit(c)) {
            ++argo_chars_read;
            ++count;
            n->float_value = n->float_value * 10 + (c - ARGO_DIGIT0);
            temp_ldouble = temp_ldouble * 10 + (c - ARGO_DIGIT0);
            if(negative && temp_ldouble > DBL_MAX)
                float_underflow = 1;
            if(!negative && temp_ldouble > DBL_MAX)
                float_overflow = 1;
            argo_append_char(&n->string_value, c);
            c = fgetc(f);
        }
        for(int i = 0; i < count; ++i) {
            n->float_value /= 10;
            temp_ldouble /= 10;
            if(negative && temp_ldouble > DBL_MAX)
                float_underflow = 1;
            if(!negative && temp_ldouble > DBL_MAX)
                float_overflow = 1;
        }
    }
    if(argo_is_exponent(c)) {
        ++argo_chars_read;
        argo_append_char(&n->string_value, c);
        n->valid_int = 0;
        c = fgetc(f);
        if(eof(c)) {
            n = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(c != ARGO_MINUS && c != ARGO_PLUS && !argo_is_digit(c)) {
            if(argo_is_control(c))
                fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) in exponent.\n", argo_lines_read, argo_chars_read, c);
            else
                fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) in exponent.\n", argo_lines_read, argo_chars_read, c, c);
            n = NULL;
            return -1;
        }
        argo_append_char(&n->string_value, c);
        if(c == ARGO_MINUS) {
            c = fgetc(f);
            if(eof(c)) {
                n = NULL;
                return -1;
            }
            ++argo_chars_read;
            if(!argo_is_digit(c)) {
                if(argo_is_control(c))
                    fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got control character (%d) for exponent.\n", argo_lines_read, argo_chars_read, c);
                else
                    fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got '%c' (%d) for exponent.\n", argo_lines_read, argo_chars_read, c, c);
                n = NULL;
                return -1;
            }
            argo_append_char(&n->string_value, c);
            unsigned long long exp = c - ARGO_DIGIT0;
            c = fgetc(f);
            while(argo_is_digit(c)) {
                exp = exp * 10 + (c - ARGO_DIGIT0);
                if(exp - 1 > LONG_MAX) {
                    fprintf(stderr, "[%d:%d] ERROR: Underflow in exponent.\n", argo_lines_read, argo_chars_read);
                    n = NULL;
                    return -1;
                }
                ++argo_chars_read;
                argo_append_char(&n->string_value, c);
                c = fgetc(f);
            }
            for(int i = 0; i < exp; ++i) {
                n->float_value /= 10;
                temp_ldouble /= 10;
                if(negative && temp_ldouble > DBL_MAX)
                    float_underflow = 1;
                if(!negative && temp_ldouble > DBL_MAX)
                    float_overflow = 1;
            }
        }
        else {
            if(c == ARGO_PLUS) {
                c = fgetc(f);
                if(eof(c)) {
                    n = NULL;
                    return -1;
                }
                ++argo_chars_read;
                if(!argo_is_digit(c)) {
                    if(argo_is_control(c))
                        fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got control character (%d) for exponent.\n", argo_lines_read, argo_chars_read, c);
                    else
                        fprintf(stderr, "[%d:%d] ERROR: Expected a decimal digit but got '%c' (%d) for exponent.\n", argo_lines_read, argo_chars_read, c, c);
                    n = NULL;
                    return -1;
                }
                argo_append_char(&n->string_value, c);
            }
            unsigned long long exp = c - ARGO_DIGIT0;
            c = fgetc(f);
            while(argo_is_digit(c)) {
                exp = exp * 10 + (c - ARGO_DIGIT0);
                if(exp > LONG_MAX) {
                    fprintf(stderr, "[%d:%d] ERROR: Overflow in exponent.\n", argo_lines_read, argo_chars_read);
                    n = NULL;
                    return -1;
                }
                ++argo_chars_read;
                argo_append_char(&n->string_value, c);
                c = fgetc(f);
            }
            for(int i = 0; i < exp; ++i) {
                n->float_value *= 10;
                temp_ldouble *= 10;
                if(negative && temp_ldouble > DBL_MAX)
                    float_underflow = 1;
                if(!negative && temp_ldouble > DBL_MAX)
                    float_overflow = 1;
            }
        }
    }
    if(int_overflow || float_overflow) {
        fprintf(stderr, "[%d:%d] ERROR: Overflow in number.\n", argo_lines_read, argo_chars_read);
        n = NULL;
        return -1;
    }
    if(int_underflow || float_underflow) {
        fprintf(stderr, "[%d:%d] ERROR: Underflow in number.\n", argo_lines_read, argo_chars_read);
        n = NULL;
        return -1;
    }
    if(!n->valid_int)
        n->int_value = 0;
    if(negative)
        n->float_value *= -1;
    ungetc(c, f);
    return 0;
}

int argo_read_array(ARGO_ARRAY *a, FILE *f) {
    if(a == NULL || f == NULL) {
        fprintf(stderr, "ERROR: Null pointer argument.\n");
        a = NULL;
        return -1;
    }
    ARGO_CHAR c = fgetc(f);
    if(eof(c)) {
        a = NULL;
        return -1;
    }
    ++argo_chars_read;
    if(c != ARGO_LBRACK) {
        if(argo_is_control(c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) at start of value.\n", argo_lines_read, argo_chars_read, c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) at start of value.\n", argo_lines_read, argo_chars_read, c, c);
        a = NULL;
        return -1;
    }
    if(argo_next_value == NUM_ARGO_VALUES) {
        fprintf(stderr, "ERROR: Out of memory. (Value capacity: %d.)\n", NUM_ARGO_VALUES);
        a = NULL;
        return -1;
    }
    ++indent_level;
    a->element_list = argo_value_storage + argo_next_value++;
    a->element_list->next = a->element_list;
    a->element_list->prev = a->element_list;
    c = fgetc(f);
    next_char(&c, f);
    if(c == ARGO_RBRACK)
        ++argo_chars_read;
    while(c != ARGO_RBRACK) {
        if(eof(c)) {
            a = NULL;
            return -1;
        }
        ungetc(c, f);
        ARGO_VALUE *value = argo_value(f);
        if(value == NULL) {
            a = NULL;
            return -1;
        }
        add_node(&a->element_list, &value);
        c = fgetc(f);
        next_char(&c, f);
        if(eof(c)) {
            a = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(c != ARGO_COMMA && c != ARGO_RBRACK) {
            print_err(c, "array");
            a = NULL;
            return -1;
        }
        if(c == ARGO_COMMA) {
            c = fgetc(f);
            next_char(&c, f);
            if(c == ARGO_RBRACK) {
                ++argo_chars_read;
                fprintf(stderr, "[%d:%d] ERROR: Premature end of array.\n", argo_lines_read, argo_chars_read);
                a = NULL;
                return -1;
            }
        }
    }
    // If end of array, then decrement indent level by one
    --indent_level;
    return 0;
}

void add_node(ARGO_VALUE **sentinel, ARGO_VALUE **value) {
    (*sentinel)->prev->next = *value;
    (*value)->prev = (*sentinel)->prev;
    (*value)->next = *sentinel;
    (*sentinel)->prev = *value;
}

int argo_read_object(ARGO_OBJECT *o, FILE *f) {
    if(o == NULL || f == NULL) {
        fprintf(stderr, "ERROR: Null pointer argument.\n");
        o = NULL;
        return -1;
    }
    ARGO_CHAR c = fgetc(f);
    if(eof(c)) {
        o = NULL;
        return -1;
    }
    ++argo_chars_read;
    if(c != ARGO_LBRACE) {
        if(argo_is_control(c))
            fprintf(stderr, "[%d:%d] ERROR: Invalid character (%d) at start of value.\n", argo_lines_read, argo_chars_read, c);
        else
            fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (%d) at start of value.\n", argo_lines_read, argo_chars_read, c, c);
        o = NULL;
        return -1;
    }
    if(argo_next_value == NUM_ARGO_VALUES) {
        fprintf(stderr, "ERROR: Out of memory. (Value capacity: %d.)\n", NUM_ARGO_VALUES);
        o = NULL;
        return -1;
    }
    ++indent_level;
    o->member_list = argo_value_storage + argo_next_value++;
    o->member_list->next = o->member_list;
    o->member_list->prev = o->member_list;
    c = fgetc(f);
    next_char(&c, f);
    if(c == ARGO_RBRACE)
        ++argo_chars_read;
    while(c != ARGO_RBRACE) {
        if(eof(c)) {
            o = NULL;
            return -1;
        }
        if(c != ARGO_QUOTE) {
            ++argo_chars_read;
            if(argo_is_control(c))
                fprintf(stderr, "[%d:%d] ERROR: Expected '%c' (%d) but got control character (%d) for object member.\n", argo_lines_read, argo_chars_read, ARGO_QUOTE, ARGO_QUOTE, c);
            else
                fprintf(stderr, "[%d:%d] ERROR: Expected '%c' (%d) but got '%c' (%d) for object member.\n", argo_lines_read, argo_chars_read, ARGO_QUOTE, ARGO_QUOTE, c, c);
            o = NULL;
            return -1;
        }
        if(argo_next_value == NUM_ARGO_VALUES) {
            fprintf(stderr, "ERROR: Out of memory. (Value capacity: %d.)\n", NUM_ARGO_VALUES);
            o = NULL;
            return -1;
        }
        ARGO_VALUE *value = argo_value_storage + argo_next_value++;
        ungetc(c, f);
        if(argo_read_string(&value->name, f)) {
            o = NULL;
            return -1;
        }
        c = fgetc(f);
        next_char(&c, f);
        if(eof(c)) {
            o = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(c != ARGO_COLON) {
            if(argo_is_control(c))
                fprintf(stderr, "[%d:%d] ERROR: Expected '%c' (%d) but got control character (%d) for object member.\n", argo_lines_read, argo_chars_read, ARGO_COLON, ARGO_COLON, c);
            else
                fprintf(stderr, "[%d:%d] ERROR: Expected '%c' (%d) but got '%c' (%d) for object member.\n", argo_lines_read, argo_chars_read, ARGO_COLON, ARGO_COLON, c, c);
            o = NULL;
            return -1;
        }
        c = fgetc(f);
        next_char(&c, f);
        if(eof(c)) {
            o = NULL;
            return -1;
        }
        argo_value_helper(&c, &value, f);
        if(value == NULL) {
            o = NULL;
            return -1;
        }
        add_node(&o->member_list, &value);
        c = fgetc(f);
        next_char(&c, f);
        if(eof(c)) {
            o = NULL;
            return -1;
        }
        ++argo_chars_read;
        if(c != ARGO_COMMA && c != ARGO_RBRACE) {
            print_err(c, "object");
            o = NULL;
            return -1;
        }
        if(c == ARGO_COMMA) {
            c = fgetc(f);
            next_char(&c, f);
            if(c == ARGO_RBRACE) {
                ++argo_chars_read;
                fprintf(stderr, "[%d:%d] ERROR: Premature end of object.\n", argo_lines_read, argo_chars_read);
                o = NULL;
                return -1;
            }
        }
    }
    // If end of object, then decrement indent level by one
    --indent_level;
    return 0;
}

/**
 * @brief  Write canonical JSON representing a specified value to
 * a specified output stream.
 * @details  Write canonical JSON representing a specified value
 * to specified output stream.  See the assignment document for a
 * detailed discussion of the data structure and what is meant by
 * canonical JSON.
 *
 * @param v  Data structure representing a value.
 * @param f  Output stream to which JSON is to be written.
 * @return  Zero if the operation is completely successful,
 * nonzero if there is any error.
 */
int argo_write_value(ARGO_VALUE *v, FILE *f) {
    // TO BE IMPLEMENTED.
    // global_options & 0xff gives the number of spaces per indentation level
    indent_level = 0;
    return argo_write(v, f);
}

int argo_write(ARGO_VALUE *v, FILE *f) {
    if(v == NULL || f == NULL) {
        fprintf(stderr, "[Write] ERROR: Null pointer argument.\n");
        return -1;
    }
    if(v->type == ARGO_BASIC_TYPE) {
        if(!argo_write_basic(&v->content.basic, f)) {
            if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION && indent_level == 0) {
                fprintf(f, "\n");
            }
            return 0;
        }
    }
    if(v->type == ARGO_STRING_TYPE) {
        if(!argo_write_string(&v->content.string, f)) {
            if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION && indent_level == 0) {
                fprintf(f, "\n");
            }
            return 0;
        }
    }
    if(v->type == ARGO_NUMBER_TYPE) {
        if(!argo_write_number(&v->content.number, f)) {
            if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION && indent_level == 0) {
                fprintf(f, "%c", ARGO_LF);
            }
            return 0;
        }
    }
    if(v->type == ARGO_ARRAY_TYPE) {
        if(!argo_write_array(&v->content.array, f)) {
            if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION && indent_level == 0) {
                fprintf(f, "%c", ARGO_LF);
            }
            return 0;
        }
    }
    if(v->type == ARGO_OBJECT_TYPE) {
        if(!argo_write_object(&v->content.object, f)) {
            if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION && indent_level == 0) {
                fprintf(f, "%c", ARGO_LF);
            }
            return 0;
        }
    }
    fprintf(stderr, "[Write] ERROR: Could not write to stream.\n");
    return -1;
}

int argo_write_basic(ARGO_BASIC *b, FILE *f) {
    if(b == NULL || f == NULL) {
        fprintf(stderr, "[Write] ERROR: Null pointer argument.\n");
        return -1;
    }
    if(*b == ARGO_NULL) {
        fputs(ARGO_NULL_TOKEN, f);
        return 0;
    }
    if(*b == ARGO_TRUE) {
        fputs(ARGO_TRUE_TOKEN, f);
        return 0;
    }
    if(*b == ARGO_FALSE) {
        fputs(ARGO_FALSE_TOKEN, f);
        return 0;
    }
    return -1;
}

/**
 * @brief  Write canonical JSON representing a specified string
 * to a specified output stream.
 * @details  Write canonical JSON representing a specified string
 * to specified output stream.  See the assignment document for a
 * detailed discussion of the data structure and what is meant by
 * canonical JSON.  The argument string may contain any sequence of
 * Unicode code points and the output is a JSON string literal,
 * represented using only 8-bit bytes.  Therefore, any Unicode code
 * with a value greater than or equal to U+00FF cannot appear directly
 * in the output and must be represented by an escape sequence.
 * There are other requirements on the use of escape sequences;
 * see the assignment handout for details.
 *
 * @param v  Data structure representing a string (a sequence of
 * Unicode code points).
 * @param f  Output stream to which JSON is to be written.
 * @return  Zero if the operation is completely successful,
 * nonzero if there is any error.
 */
int argo_write_string(ARGO_STRING *s, FILE *f) {
    // TO BE IMPLEMENTED.
    if(s == NULL || f == NULL) {
        fprintf(stderr, "[Write] ERROR: Null pointer argument.\n");
        return -1;
    }
    fputc(ARGO_QUOTE, f);
    ARGO_CHAR c;
    for(int i = 0; i < s->length; ++i) {
        c = *(s->content + i);
        if(c > 0xffff || c < 0)
            return -1;
        if(c == ARGO_BSLASH) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_BSLASH, f);
        }
        else if(c == ARGO_QUOTE) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_QUOTE, f);
        }
        else if(c == ARGO_BS) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_B, f);
        }
        else if(c == ARGO_FF) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_F, f);
        }
        else if(c == ARGO_LF) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_N, f);
        }
        else if(c == ARGO_CR) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_R, f);
        }
        else if(c == ARGO_HT) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_T, f);
        }
        else if(c > 0x1f && c < 0xff)
            fputc(c, f);
        else if (c >= 0xff || c <= 0x1f) {
            fputc(ARGO_BSLASH, f);
            fputc(ARGO_U, f);
            int quotient;
            int remainder;
            for(int j = 4; j >= 1; --j) {
                quotient = c;
                for(int k = 0; k < j; ++k) {
                    remainder = quotient % 16;
                    quotient /= 16;
                }
                if(remainder == 0xa)
                    fputc('a', f);
                else if(remainder == 0xb)
                    fputc('b', f);
                else if(remainder == 0xc)
                    fputc('c', f);
                else if(remainder == 0xd)
                    fputc('d', f);
                else if(remainder == 0xe)
                    fputc('e', f);
                else if(remainder == 0xf)
                    fputc('f', f);
                else fputc(remainder + ARGO_DIGIT0, f);
            }
        }
    }
    fputc(ARGO_QUOTE, f);
    return 0;
}

/**
 * @brief  Write canonical JSON representing a specified number
 * to a specified output stream.
 * @details  Write canonical JSON representing a specified number
 * to specified output stream.  See the assignment document for a
 * detailed discussion of the data structure and what is meant by
 * canonical JSON.  The argument number may contain representations
 * of the number as any or all of: string conforming to the
 * specification for a JSON number (but not necessarily canonical),
 * integer value, or floating point value.  This function should
 * be able to work properly regardless of which subset of these
 * representations is present.
 *
 * @param v  Data structure representing a number.
 * @param f  Output stream to which JSON is to be written.
 * @return  Zero if the operation is completely successful,
 * nonzero if there is any error.
 */
int argo_write_number(ARGO_NUMBER *n, FILE *f) {
    // TO BE IMPLEMENTED.
    if(n == NULL || f == NULL) {
        fprintf(stderr, "[Write] ERROR: Null pointer argument.\n");
        return -1;
    }
    if(n->valid_int) {
        long num = n->int_value;
        if(num < 0) {
            fputc(ARGO_MINUS, f);
            num *= -1;
        }
        long div = 1;
        while((num / div) >= 10)
            div *= 10;
        while(div > 0) {
            fputc(((num / div) % 10) + ARGO_DIGIT0, f);
            div /= 10;
        }
        return 0;
    }
    else if(n->valid_float) {
        double num = n->float_value;
        if(num < 0) {
            fputc(ARGO_MINUS, f);
            num *= -1;
        }
        long count = 0;
        if(num > 0) {
            while(num >= 1) {
                ++count;
                num /= 10;
            }
            while(num < 0.1) {
                --count;
                num *= 10;
            }
        }
        else if(num < 0) {
            while(num <= -1) {
                ++count;
                num /= 10;
            }
            while(num > -0.1) {
                --count;
                num *= 10;
            }
        }
        fputc(ARGO_DIGIT0, f);
        fputc(ARGO_PERIOD, f);
        for(int i = 0; i < ARGO_PRECISION; ++i) {
            num *= 10;
            fputc((int) num + ARGO_DIGIT0, f);
            num -= (int) num;
            if(num == 0)
                break;
        }
        if(count) {
            fputc(ARGO_E, f);
            if(count < 0) {
                fputc(ARGO_MINUS, f);
                count *= -1;
            }
            long div = 1;
            while((count / div) >= 10)
                div *= 10;
            while(div > 0) {
                fputc(((count / div) % 10) + ARGO_DIGIT0, f);
                div /= 10;
            }
        }
        return 0;
    }
    return -1;
}

int argo_write_array(ARGO_ARRAY *a, FILE *f) {
    if(a == NULL || f == NULL) {
        fprintf(stderr, "[Write] ERROR: Null pointer argument.\n");
        return -1;
    }
    fputc(ARGO_LBRACK, f);
    if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION) {
        fputc(ARGO_LF, f);
        if(a->element_list->next != a->element_list)
            ++indent_level;
        int num_of_spaces = indent_level * (global_options & 0xff);
        for(int i = 0; i < num_of_spaces; ++i)
            fputc(ARGO_SPACE, f);
    }
    if(a->element_list->next == a->element_list) {
        fputc(ARGO_RBRACK, f);
        return 0;
    }
    ARGO_VALUE *ptr = a->element_list->next;
    while(ptr != a->element_list->prev) {
        if(argo_write(ptr, f))
            return -1;
        fputc(ARGO_COMMA, f);
        if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION) {
            fputc(ARGO_LF, f);
            int num_of_spaces = indent_level * (global_options & 0xff);
            for(int i = 0; i < num_of_spaces; ++i)
                fputc(ARGO_SPACE, f);
        }
        ptr = ptr->next;
    }
    if(argo_write(ptr, f))
        return -1;
    if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION) {
        fputc(ARGO_LF, f);
        --indent_level;
        int num_of_spaces = indent_level * (global_options & 0xff);
        for(int i = 0; i < num_of_spaces; ++i)
            fputc(ARGO_SPACE, f);
    }
    fputc(ARGO_RBRACK, f);
    return 0;
}

int argo_write_object(ARGO_OBJECT *o, FILE *f) {
    if(o == NULL || f == NULL) {
        fprintf(stderr, "[Write] ERROR: Null pointer argument.\n");
        return -1;
    }
    fputc(ARGO_LBRACE, f);
    if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION) {
        fputc(ARGO_LF, f);
        if(o->member_list->next != o->member_list)
            ++indent_level;
        int num_of_spaces = indent_level * (global_options & 0xff);
        for(int i = 0; i < num_of_spaces; ++i)
            fputc(ARGO_SPACE, f);
    }
    if(o->member_list->next == o->member_list) {
        fputc(ARGO_RBRACE, f);
        return 0;
    }
    ARGO_VALUE *ptr = o->member_list->next;
    while(ptr != o->member_list->prev) {
        if(argo_write_string(&ptr->name, f))
            return -1;
        fputc(ARGO_COLON, f);
        if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION)
            fputc(ARGO_SPACE, f);
        if(argo_write(ptr, f))
            return -1;
        fputc(ARGO_COMMA, f);
        if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION) {
            fputc(ARGO_LF, f);
            int num_of_spaces = indent_level * (global_options & 0xff);
            for(int i = 0; i < num_of_spaces; ++i)
                fputc(ARGO_SPACE, f);
        }
        ptr = ptr->next;
    }
    if(argo_write_string(&ptr->name, f))
        return -1;
    fputc(ARGO_COLON, f);
    if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION)
        fputc(ARGO_SPACE, f);
    if(argo_write(ptr, f))
        return -1;
    if((global_options & PRETTY_PRINT_OPTION) == PRETTY_PRINT_OPTION) {
        fputc(ARGO_LF, f);
        --indent_level;
        int num_of_spaces = indent_level * (global_options & 0xff);
        for(int i = 0; i < num_of_spaces; ++i)
            fputc(ARGO_SPACE, f);
    }
    fputc(ARGO_RBRACE, f);
    return 0;
}
