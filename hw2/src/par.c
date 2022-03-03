/*********************/
/* par.c             */
/* for Par 3.20      */
/* Copyright 1993 by */
/* Adam M. Costello  */
/*********************/

/* This is ANSI C code. */


#include "errmsg.h"
#include "buffer.h"    /* Also includes <stddef.h>. */
#include "reformat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>

#undef NULL
#define NULL ((void *) 0)


const char * const progname = "par";
const char * const version = "3.20";


static int digtoint(char c)

/* Returns the value represented by the digit c,   */
/* or -1 if c is not a digit. Does not use errmsg. */
{
  return c == '0' ? 0 :
         c == '1' ? 1 :
         c == '2' ? 2 :
         c == '3' ? 3 :
         c == '4' ? 4 :
         c == '5' ? 5 :
         c == '6' ? 6 :
         c == '7' ? 7 :
         c == '8' ? 8 :
         c == '9' ? 9 :
         -1;

  /* We can't simply return c - '0' because this is ANSI  */
  /* C code, so it has to work for any character set, not */
  /* just ones which put the digits together in order.    */
}


static int strtoudec(const char *s, int *pn)

/* Puts the decimal value of the string s into *pn, returning */
/* 1 on success. If s is empty, or contains non-digits,       */
/* or represents an integer greater than 9999, then *pn       */
/* is not changed and 0 is returned. Does not use errmsg.     */
{
  int n = 0;

  if (!*s) return 0;

  do {
    if (n >= 1000 || !isdigit(*s)) return 0;
    n = 10 * n + digtoint(*s);
  } while (*++s);

  *pn = n;

  return 1;
}


static void parseopt(
  const char *opt, int *pwidth, int *pprefix,
  int *psuffix, int *phang, int *plast, int *pmin
)
/* Parses the single option in opt, setting *pwidth, *pprefix,     */
/* *psuffix, *phang, *plast, or *pmin as appropriate. Uses errmsg. */
{
  const char *saveopt = opt;
  char oc;
  int n, r;
  char *err = NULL;
  size_t size = 0;
  FILE *memstream = NULL;

  if (*opt == '-') ++opt;

  if (!strcmp(opt, "version")) {
    err = NULL;
    size = 0;
    memstream = open_memstream(&err, &size);
    fprintf(memstream, "%s %s\n", progname, version);
    fflush(memstream);
    clear_error();
    set_error(err);
    if (memstream) fclose(memstream);
    if (err) free(err);
    return;
  }

  oc = *opt;

  if (isdigit(oc)) {
    if (!strtoudec(opt, &n)) goto badopt;
    if (n <= 8) *pprefix = n;
    else *pwidth = n;
  }
  else {
    if (!oc) goto badopt;
    n = 1;
    r = strtoudec(opt + 1, &n);
    if (opt[1] && !r) goto badopt;

    if (oc == 'w' || oc == 'p' || oc == 's') {
      if (!r) goto badopt;
      if      (oc == 'w') *pwidth  = n;
      else if (oc == 'p') *pprefix = n;
      else                *psuffix = n;
    }
    else if (oc == 'h') *phang = n;
    else if (n <= 1) {
      if      (oc == 'l') *plast = n;
      else if (oc == 'm') *pmin = n;
    }
    else goto badopt;
  }

  clear_error();
  return;

badopt:
  err = NULL;
  size = 0;
  memstream = open_memstream(&err, &size);
  fprintf(memstream, "Bad option: '%s'\n", saveopt);
  fflush(memstream);
  clear_error();
  set_error(err);
  if (memstream) fclose(memstream);
  if (err) free(err);
}


static char **readlines(void)

/* Reads lines from stdin until EOF, or until a blank line is encountered, */
/* in which case the newline is pushed back onto the input stream. Returns */
/* a NULL-terminated array of pointers to individual lines, stripped of    */
/* their newline characters. Uses errmsg, and returns NULL on failure.     */
{
  struct buffer *cbuf = NULL, *pbuf = NULL;
  int c, blank;
  char ch, *ln = NULL, *nullline = NULL, nullchar = '\0', **lines = NULL;

  cbuf = newbuffer(sizeof (char));
  if (is_error()) goto rlcleanup;
  pbuf = newbuffer(sizeof (char *));
  if (is_error()) goto rlcleanup;

  for (blank = 1;  ; ) {
    c = getchar();
    if (c == EOF) break;
    if (c == '\n') {
      if (blank) {
        ungetc(c,stdin);
        break;
      }
      additem(cbuf, &nullchar);
      if (is_error()) goto rlcleanup;
      ln = copyitems(cbuf);
      if (is_error()) goto rlcleanup;
      additem(pbuf, &ln);
      if (is_error()) goto rlcleanup;
      clearbuffer(cbuf);
      blank = 1;
    }
    else {
      if (!isspace(c)) blank = 0;
      ch = c;
      additem(cbuf, &ch);
      if (is_error()) goto rlcleanup;
    }
  }

  if (!blank) {
    additem(cbuf, &nullchar);
    if (is_error()) goto rlcleanup;
    ln = copyitems(cbuf);
    if (is_error()) goto rlcleanup;
    additem(pbuf, &ln);
    if (is_error()) goto rlcleanup;
  }

  additem(pbuf, &nullline);
  if (is_error()) goto rlcleanup;
  lines = copyitems(pbuf);

rlcleanup:

  if (cbuf) freebuffer(cbuf);
  if (pbuf) {
    freebuffer(pbuf);
    if (!lines)
      for (;;) {
        lines = nextitem(pbuf);
        if (!lines) break;
        free(*lines);
      }
  }

  return lines;
}


static void setdefaults(
  const char * const *inlines, int *pwidth, int *pprefix,
  int *psuffix, int *phang, int *plast, int *pmin
)
/* If any of *pwidth, *pprefix, *psuffix, *phang, *plast, *pmin are     */
/* less than 0, sets them to default values based on inlines, according */
/* to "par.doc". Does not use errmsg because it always succeeds.        */
{
  int numlines;
  const char *start = NULL, *end = NULL, * const *line = NULL, *p1 = NULL, *p2 = NULL;

  if (*pwidth < 0) *pwidth = 72;
  if (*phang < 0) *phang = 0;
  if (*plast < 0) *plast = 0;
  if (*pmin < 0) *pmin = *plast;

  for (line = inlines;  *line;  ++line);
  numlines = line - inlines;

  if (*pprefix < 0) {
    if (numlines <= *phang + 1)
      *pprefix = 0;
    else {
      start = inlines[*phang];
      for (end = start;  *end;  ++end);
      for (line = inlines + *phang + 1;  *line;  ++line) {
        for (p1 = start, p2 = *line;  p1 < end && *p1 == *p2;  ++p1, ++p2);
        end = p1;
      }
      *pprefix = end - start;
    }
  }

  if (*psuffix < 0) {
    if (numlines <= 1)
      *psuffix = 0;
    else {
      start = *inlines;
      for (end = start;  *end;  ++end);
      for (line = inlines + 1;  *line;  ++line) {
        for (p2 = *line;  *p2;  ++p2);
        for (p1 = end; p1 > start && p2 > *line && p1[-1] == p2[-1]; --p1, --p2);
        start = p1;
      }
      while (end - start >= 2 && isspace(*start) && isspace(start[1])) ++start;
      *psuffix = end - start;
    }
  }
}

static int only_digits(char *s)
{
  for (int i = 0; i < strlen(s); ++i) {
    if (!isdigit(s[i])) return 0;
  }
  return 1;
}


static void freelines(char **lines)
/* Frees the strings pointed to in the NULL-terminated array lines, then */
/* frees the array. Does not use errmsg because it always succeeds.      */
{
  for (int i = 0; lines[i]; ++i)
    free(lines[i]);

  free(lines);
}

static void getoptlong_parse(int *ver, int *flag, char * const *argv, int argc, int *widthbak, int *prefixbak,
  int *suffixbak, int *hangbak, int *lastbak, int *minbak)
{
  int op = 0, r = 0;
  char *err = NULL;
  size_t size = 0;
  FILE *memstream = NULL;

  while (1) {
    static struct option long_options[] =
      {
        {"version", no_argument, 0, 'v'},
        {"width", required_argument, 0, 'w'},
        {"prefix", required_argument, 0, 'p'},
        {"suffix", required_argument, 0, 's'},
        {"hang", optional_argument, 0, 'h'},
        {"last", no_argument, 0, 'a'},
        {"min", no_argument, 0, 'i'},
        {"no-last", no_argument, 0, 't'},
        {"no-min", no_argument, 0, 'n'},
        {0, 0, 0, 0}
      };
    int option_index = 0;

    op = getopt_long (argc, argv, "-w:p:s:h::l::m::", long_options, &option_index);
    if (op == -1)
      break;
    switch (op) {
      case 'v':
        err = NULL;
        size = 0;
        memstream = open_memstream(&err, &size);
        fprintf(memstream, "%s %s\n", progname, version);
        fflush(memstream);
        clear_error();
        set_error(err);
        if (memstream) fclose(memstream);
        if (err) free(err);
        *ver = 1;
        break;

      case 'w':
        if(!strtoudec(optarg, &r)) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        *widthbak = r;
        break;

      case 'p':
        if(!strtoudec(optarg, &r)) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        *prefixbak = r;
        break;

      case 's':
        if(!strtoudec(optarg, &r)) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        *suffixbak = r;
        break;

      case 'h':
        /** The long form of the option 'hang' may have optional arguments,
         *  and thus, should work with whitespace (as specified in the HW doc)
         *  between the option (long-form) and the option argument.
         *  However, setting the field has_arg to optional_argument for the
         *  'hang' option causes getopt_long() to set the optarg to the option
         *  argument only if there is no whitespace between the option (long-form)
         *  and the argument. To bypass this, we use the following condition:
         */
        if (optarg == NULL && optind < argc && argv[optind][0] != '-') {
          optarg = argv[optind++];
        }
        if (optarg == NULL) break;
        if(!strtoudec(optarg, &r)) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        *hangbak = r;
        break;

      case 'l':
        if (optarg == NULL && optind < argc && argv[optind][0] != '-') {
          optarg = argv[optind++];
        }
        if (optarg == NULL) break;
        if(!strtoudec(optarg, &r)) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        if (r != 0 && r != 1) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        *lastbak = r;
        break;

      case 'm':
        if (optarg == NULL && optind < argc && argv[optind][0] != '-') {
          optarg = argv[optind++];
        }
        if (optarg == NULL) break;
        if(!strtoudec(optarg, &r)) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        if (r != 0 && r != 1) {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option argument: '%s'\n", optarg);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
        *minbak = r;
        break;

      case 'a':
        *lastbak = 1;
        break;

      case 'i':
        *minbak = 1;
        break;

      case 't':
        *lastbak = 0;
        break;

      case 'n':
        *minbak = 0;
        break;

      case '?':
        *flag = 1;
        *ver = 0;
        return;

      default:
        if (only_digits(argv[optind - 1])) {
          if (!strtoudec(argv[optind - 1], &r)) {
            err = NULL;
            size = 0;
            memstream = open_memstream(&err, &size);
            fprintf(memstream, "Bad argument: '%s'\n", argv[optind - 1]);
            fflush(memstream);
            clear_error();
            set_error(err);
            if (memstream) fclose(memstream);
            if (err) free(err);
            *ver = 0;
            return;
          }
          if (r <= 8) *prefixbak = r;
          else *widthbak = r;
        }
        else {
          err = NULL;
          size = 0;
          memstream = open_memstream(&err, &size);
          fprintf(memstream, "Bad option: '%s'\n", argv[optind - 1]);
          fflush(memstream);
          clear_error();
          set_error(err);
          if (memstream) fclose(memstream);
          if (err) free(err);
          *ver = 0;
          return;
        }
    }
  }
}


int original_main(int argc, char * const *argv)
{
  int width, widthbak = -1, prefix, prefixbak = -1, suffix, suffixbak = -1,
      hang, hangbak = -1, last, lastbak = -1, min, minbak = -1, c, flag = 0, ver = 0;
  char *parinit, *picopy = NULL, *opt, **inlines = NULL, **outlines = NULL,
       **line;
  const char * const whitechars = " \f\n\r\t\v";
  char **vars = NULL;

  parinit = getenv("PARINIT");
  if (parinit) {
    picopy = calloc((strlen(parinit) + 1), sizeof (char));
    if (!picopy) {
      clear_error();
      set_error("Out of memory.\n");
      goto parcleanup;
    }
    strcpy(picopy,parinit);
    opt = strtok(picopy,whitechars);
    int num_of_vars = 1;
    if (opt)
      vars = malloc(sizeof(char *));
    while (opt) {
      //parseopt(opt, &widthbak, &prefixbak,
               //&suffixbak, &hangbak, &lastbak, &minbak);
      vars[num_of_vars - 1] = opt;
      opt = strtok(NULL,whitechars);
      if (opt)
        vars = realloc(vars, (++num_of_vars) * sizeof(char *));
    }
    flag = 0;
    ver = 0;
    getoptlong_parse(&ver, &flag, vars, num_of_vars, &widthbak, &prefixbak, &suffixbak, &hangbak, &lastbak, &minbak);
    if (flag) goto parcleanup;
    if (is_error() && !ver) goto parcleanup;
    if (picopy) {
      free(picopy);
      picopy = NULL;
    }
    if (vars) {
      free(vars);
      vars = NULL;
    }
  }

  //while (*++argv) {
    //parseopt(*argv, &widthbak, &prefixbak,
             //&suffixbak, &hangbak, &lastbak, &minbak);
    //if (*errmsg) goto parcleanup;
  //}
  flag = 0;
  optind = 1; // The initial value of optind is set to one
  getoptlong_parse(&ver, &flag, argv, argc, &widthbak, &prefixbak, &suffixbak, &hangbak, &lastbak, &minbak);
  if (is_error() || flag) goto parcleanup;

  for (;;) {
    for (;;) {
      c = getchar();
      if (c != '\n') break;
      putchar(c);
    }
    ungetc(c,stdin);

    inlines = readlines();
    if (is_error()) goto parcleanup;
    if (!*inlines) {
      free(inlines);
      inlines = NULL;
      break;
    }

    width = widthbak;  prefix = prefixbak;  suffix = suffixbak;
    hang = hangbak;  last = lastbak;  min = minbak;
    setdefaults((const char * const *) inlines,
                &width, &prefix, &suffix, &hang, &last, &min);

    outlines = reformat((const char * const *) inlines,
                        width, prefix, suffix, hang, last, min);
    if (is_error()) goto parcleanup;

    freelines(inlines);
    inlines = NULL;

    for (line = outlines;  *line;  ++line)
      puts(*line);

    freelines(outlines);
    outlines = NULL;
  }

parcleanup:

  if (picopy) free(picopy);
  if (vars) free(vars);
  if (inlines) freelines(inlines);
  if (outlines) freelines(outlines);

  if (is_error()) {
    if (!flag) report_error(stderr);
    clear_error();
    if (!flag && ver) return EXIT_SUCCESS;
    return EXIT_FAILURE;
  }
  if (flag) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}