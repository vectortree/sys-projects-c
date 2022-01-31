#include <stdlib.h>
#include <stdio.h>

#include "argo.h"
#include "global.h"
#include "debug.h"

ARGO_VALUE *argo_read_value(FILE *);
void init_vars();
void next_char(ARGO_CHAR *, FILE *);
ARGO_VALUE *argo_value(FILE *);
int eof(ARGO_CHAR);
void print_char_err(ARGO_CHAR, ARGO_CHAR, char *);
int argo_read_basic(ARGO_BASIC *, FILE *);
int argo_read_string(ARGO_STRING *, FILE *);
int argo_read_number(ARGO_NUMBER *, FILE *);
int argo_read_array(ARGO_ARRAY *, FILE *);
int argo_read_object(ARGO_OBJECT *, FILE *);
int argo_read_member(ARGO_VALUE *, FILE *);
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
    ++argo_chars_read;
    // ARGO_BASIC
    if(c == ARGO_T || c == ARGO_F || c == ARGO_N) {
        ungetc(c, f);
        if(argo_read_basic(&value->content.basic, f))
            return NULL;
        value->type = ARGO_BASIC_TYPE;
        return value;
    }
    // ARGO_STRING
    if(c == ARGO_QUOTE) {
        if(argo_read_string(&value->content.string, f))
            return NULL;
        value->type = ARGO_STRING_TYPE;
        return value;
    }
    // ARGO_NUMBER
    if(argo_is_digit(c) || c == ARGO_MINUS) {
        ungetc(c, f);
        if(argo_read_number(&value->content.number, f))
            return NULL;
        value->type = ARGO_NUMBER_TYPE;
        return value;
    }
    // ARGO_ARRAY
    if(c == ARGO_LBRACK) {
        if(argo_read_array(&value->content.array, f))
            return NULL;
        value->type = ARGO_ARRAY_TYPE;
        return value;
    }
    // ARGO_OBJECT
    if(c == ARGO_LBRACE) {
        ++argo_chars_read;
        if(argo_read_object(&value->content.object, f))
            return NULL;
        value->type = ARGO_OBJECT_TYPE;
        return value;
    }
    fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' at start of value.\n", argo_lines_read, argo_chars_read, c);
    return NULL;
}

int eof(ARGO_CHAR c) {
    if(c == EOF) {
        fprintf(stderr, "[%d:%d] ERROR: Premature EOF.\n", argo_lines_read, argo_chars_read);
        return 1;
    }
    return 0;
}

void print_char_err(ARGO_CHAR c1, ARGO_CHAR c2, char *c) {
    fprintf(stderr, "[%d:%d] ERROR: Expected '%c' but got '%c' (for token '%s').\n", argo_lines_read, argo_chars_read, c1, c2, c);
}

int argo_read_basic(ARGO_BASIC *b, FILE *f) {
    ARGO_CHAR c = fgetc(f);
    if(c == ARGO_N) {
        for(int i = 1; *(ARGO_NULL_TOKEN + i) != '\0'; ++i) {
            c = fgetc(f);
            if(eof(c))
                return -1;
            ++argo_chars_read;
            if(c != *(ARGO_NULL_TOKEN + i)) {
                print_char_err(*(ARGO_NULL_TOKEN + i), c, ARGO_NULL_TOKEN);
                return -1;
            }
        }
        c = fgetc(f);
        next_char(&c, f);
        if((indent_level == 0 && c == EOF) || (indent_level > 0 && (c == ARGO_RBRACE || c == ARGO_RBRACK || c == ARGO_COMMA))) {
            *b = ARGO_NULL;
            return 0;
        }
        if(eof(c))
            return -1;
        ++argo_chars_read;
        fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (after token '%s').\n", argo_lines_read, argo_chars_read, c, ARGO_NULL_TOKEN);
        return -1;
    }
    if(c == ARGO_T) {
        for(int i = 1; *(ARGO_TRUE_TOKEN + i) != '\0'; ++i) {
            c = fgetc(f);
            if(eof(c))
                return -1;
            ++argo_chars_read;
            if(c != *(ARGO_TRUE_TOKEN + i)) {
                print_char_err(*(ARGO_TRUE_TOKEN + i), c, ARGO_TRUE_TOKEN);
                return -1;
            }
        }
        c = fgetc(f);
        next_char(&c, f);
        if((indent_level == 0 && c == EOF) || (indent_level > 0 && (c == ARGO_RBRACE || c == ARGO_RBRACK || c == ARGO_COMMA))) {
            *b = ARGO_TRUE;
            return 0;
        }
        if(eof(c))
            return -1;
        ++argo_chars_read;
        fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (after token '%s').\n", argo_lines_read, argo_chars_read, c, ARGO_TRUE_TOKEN);
        return -1;
    }
    if(c == ARGO_F) {
        for(int i = 1; *(ARGO_FALSE_TOKEN + i) != '\0'; ++i) {
            c = fgetc(f);
            if(eof(c))
                return -1;
            ++argo_chars_read;
            if(c != *(ARGO_FALSE_TOKEN + i)) {
                print_char_err(*(ARGO_FALSE_TOKEN + i), c, ARGO_FALSE_TOKEN);
                return -1;
            }
        }
        c = fgetc(f);
        next_char(&c, f);
        if((indent_level == 0 && c == EOF) || (indent_level > 0 && (c == ARGO_RBRACE || c == ARGO_RBRACK || c == ARGO_COMMA))) {
            *b = ARGO_FALSE;
            return 0;
        }
        if(eof(c))
            return -1;
        ++argo_chars_read;
        fprintf(stderr, "[%d:%d] ERROR: Invalid character '%c' (after token '%s').\n", argo_lines_read, argo_chars_read, c, ARGO_FALSE_TOKEN);
        return -1;
    }
    return -1;
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
    return 0;
}

int argo_read_array(ARGO_ARRAY *a, FILE *f) {
    ++indent_level;
    return 0;
}

int argo_read_object(ARGO_OBJECT *o, FILE *f) {
    ++indent_level;
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
/*int argo_write_value(ARGO_VALUE *v, FILE *f) {
    // TO BE IMPLEMENTED.
    if(v->type == ARGO_BASIC_TYPE)
        return argo_write_basic(&v->content.basic, f);
    if(v->type == ARGO_STRING_TYPE)
        return argo_write_string(&v->content.string, f);
    if(v->type == ARGO_NUMBER_TYPE)
        return argo_write_number(&v->content.number, f);
    if(v->type == ARGO_ARRAY_TYPE)
        return argo_write_array(&v->content.array, f);
    if(v->type == ARGO_OBJECT_TYPE)
        return argo_write_object(&v->content.object, f);
    return -1;
}*/

int argo_write_basic(ARGO_BASIC *b, FILE *f) {
    if(*b == ARGO_NULL) {
        fprintf(f, "%s", ARGO_NULL_TOKEN);
        return 0;
    }
    if(*b == ARGO_TRUE) {
        fprintf(f, "%s", ARGO_TRUE_TOKEN);
        return 0;
    }
    if(*b == ARGO_FALSE) {
        fprintf(f, "%s", ARGO_FALSE_TOKEN);
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
    return 0;
}

int argo_write_array(ARGO_ARRAY *a, FILE *f) {
    return 0;
}

int argo_write_object(ARGO_OBJECT *o, FILE *f) {
    return 0;
}
