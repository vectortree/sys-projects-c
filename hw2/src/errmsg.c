/*********************/
/* errmsg.c          */
/* for Par 3.20      */
/* Copyright 1993 by */
/* Adam M. Costello  */
/*********************/

/* This is ANSI C code. */

#include <string.h>
#include "errmsg.h"  /* Makes sure we're consistent with the declarations. */
#include <stdio.h>
#include <stdlib.h>

static char *errmsg = NULL;

void set_error(char *msg) {
	if (!msg) return;
	errmsg = strdup(msg);
}

int is_error() {
	if (!errmsg) return 0;
	else return 1;
}

int report_error(FILE *file) {
	if (!file) return -1;
	if (!is_error()) return 0;
	if (fputs(errmsg, file) < 0) return -1;
	return 0;
}

void clear_error() {
	if (is_error()) {
		free(errmsg);
		errmsg = NULL;
	}
}
