#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/*
 * This is the "data store" module for Mush.
 * It maintains a mapping from variable names to values.
 * The values of variables are stored as strings.
 * However, the module provides functions for setting and retrieving
 * the value of a variable as an integer.  Setting a variable to
 * an integer value causes the value of the variable to be set to
 * a string representation of that integer.  Retrieving the value of
 * a variable as an integer is possible if the current value of the
 * variable is the string representation of an integer.
 */
struct variable {
    char *name;
    char *value;
};

struct ds_node {
    struct variable var;
    struct ds_node *next;
    struct ds_node *prev;
};

static struct {
    struct ds_node *sentinel;
} DATA_STORE;

/**
 * @brief  Get the current value of a variable as a string.
 * @details  This function retrieves the current value of a variable
 * as a string.  If the variable has no value, then NULL is returned.
 * Any string returned remains "owned" by the data store module;
 * the caller should not attempt to free the string or to use it
 * after any subsequent call that would modify the value of the variable
 * whose value was retrieved.  If the caller needs to use the string for
 * an indefinite period, a copy should be made immediately.
 *
 * @param  var  The variable whose value is to be retrieved.
 * @return  A string that is the current value of the variable, if any,
 * otherwise NULL.
 */
char *store_get_string(char *var) {
    // TO BE IMPLEMENTED
    if(DATA_STORE.sentinel == NULL || var == NULL) return NULL;
    struct ds_node *node = DATA_STORE.sentinel->next;
    while(node != DATA_STORE.sentinel) {
        if(strcmp(node->var.name, var) == 0) return node->var.value;
        node = node->next;
    }
    return NULL;
}

/**
 * @brief  Get the current value of a variable as an integer.
 * @details  This retrieves the current value of a variable and
 * attempts to interpret it as an integer.  If this is possible,
 * then the integer value is stored at the pointer provided by
 * the caller.
 *
 * @param  var  The variable whose value is to be retrieved.
 * @param  valp  Pointer at which the returned value is to be stored.
 * @return  If the specified variable has no value or the value
 * cannot be interpreted as an integer, then -1 is returned,
 * otherwise 0 is returned.
 */
int store_get_int(char *var, long *valp) {
    // TO BE IMPLEMENTED
    char *value = store_get_string(var);
    if(value == NULL) return -1;
    char *endptr;
    *valp = strtol(value, &endptr, 10);
    if(*endptr != '\0' || errno == ERANGE || *valp > INT_MAX || *valp < INT_MIN)
        return -1;
    return 0;
}

/**
 * @brief  Set the value of a variable as a string.
 * @details  This function sets the current value of a specified
 * variable to be a specified string.  If the variable already
 * has a value, then that value is replaced.  If the specified
 * value is NULL, then any existing value of the variable is removed
 * and the variable becomes un-set.  Ownership of the variable and
 * the value strings is not transferred to the data store module as
 * a result of this call; the data store module makes such copies of
 * these strings as it may require.
 *
 * @param  var  The variable whose value is to be set.
 * @param  val  The value to set, or NULL if the variable is to become
 * un-set.
 */
int store_set_string(char *var, char *val) {
    // TO BE IMPLEMENTED
    if(var == NULL) return -1;
    if(DATA_STORE.sentinel == NULL) {
        DATA_STORE.sentinel = malloc(sizeof(struct ds_node));
        if(DATA_STORE.sentinel == NULL) return -1;
        DATA_STORE.sentinel->next = DATA_STORE.sentinel;
        DATA_STORE.sentinel->prev = DATA_STORE.sentinel;
    }
    struct ds_node *node = DATA_STORE.sentinel->next;
    while(node != DATA_STORE.sentinel) {
        if(strcmp(node->var.name, var) == 0) {
            if(node->var.value != NULL)
                free(node->var.value);
            if(val != NULL) {
                node->var.value = malloc(strlen(val) + 1);
                if(node->var.value == NULL) return -1;
                strcpy(node->var.value, val);
            }
            else {
                node->var.value = NULL;
                node->prev->next = node->next;
                node->next->prev = node->prev;
            }
            return 0;
        }
        node = node->next;
    }
    if(val == NULL) return 0;
    node = malloc(sizeof(struct ds_node));
    if(node == NULL) return -1;
    node->var.name = malloc(strlen(var) + 1);
    if(node->var.name == NULL) return -1;
    strcpy(node->var.name, var);
    node->var.value = malloc(strlen(val) + 1);
    if(node->var.value == NULL) return -1;
    strcpy(node->var.value, val);
    node->next = DATA_STORE.sentinel;
    node->prev = DATA_STORE.sentinel->prev;
    DATA_STORE.sentinel->prev->next = node;
    DATA_STORE.sentinel->prev = node;
    return 0;
}

/**
 * @brief  Set the value of a variable as an integer.
 * @details  This function sets the current value of a specified
 * variable to be a specified integer.  If the variable already
 * has a value, then that value is replaced.  Ownership of the variable
 * string is not transferred to the data store module as a result of
 * this call; the data store module makes such copies of this string
 * as it may require.
 *
 * @param  var  The variable whose value is to be set.
 * @param  val  The value to set.
 */
int store_set_int(char *var, long val) {
    // TO BE IMPLEMENTED
    if(val > INT_MAX || val < INT_MIN) return -1;
    int length = 0;
    long copy_of_val = val;
    while(copy_of_val != 0) {
        copy_of_val /= 10;
        ++length;
    }
    char *str_val = malloc(length + 1);
    if(str_val == NULL) return -1;
    sprintf(str_val, "%ld", val);
    int return_val = store_set_string(var, str_val);
    free(str_val);
    return return_val;
}

/**
 * @brief  Print the current contents of the data store.
 * @details  This function prints the current contents of the data store
 * to the specified output stream.  The format is not specified; this
 * function is intended to be used for debugging purposes.
 *
 * @param f  The stream to which the store contents are to be printed.
 */
void store_show(FILE *f) {
    // TO BE IMPLEMENTED
    if(f == NULL) return;
    fprintf(f, "{");
    if(DATA_STORE.sentinel != NULL) {
        struct ds_node *node = DATA_STORE.sentinel->next;
        while(node != DATA_STORE.sentinel->prev) {
            fprintf(f, "%s=%s,", node->var.name, node->var.value);
            node = node->next;
        }
        if(node != DATA_STORE.sentinel)
            fprintf(f, "%s=%s", node->var.name, node->var.value);
    }
    fprintf(f, "}");
}
