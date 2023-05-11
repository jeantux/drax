#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "deval.h"
#include "dstring.h"

static void print_list(drax_list* l) {
  putchar('[');
  int i;
  for (i = 0; i < l->length; i++) {
    print_drax(l->elems[i], 1);

    if ((i+1) < l->length) printf(", ");
  }
  putchar(']');
}

static void print_frame(drax_frame* f) {
  putchar('{');
  int i;
  for (i = 0; i < f->length; i++) {
    printf("%s: ", f->literals[i]);
    print_drax(f->values[i], 1);

    if ((i+1) < f->length) printf(", ");
  }
  putchar('}');
}

static void print_string(const char* str, int formated) {
  if (!formated) {
    char* tmpstr = strndup(str, strlen(str));

    if (tmpstr == NULL) {
      printf("error");
      return;
    }

    int i, j;
    for (i = 0, j = 0; tmpstr[i] != '\0'; i++, j++) {
      if (tmpstr[i] == '\\') {
        if (tmpstr[i+1] == 'n') {
          tmpstr[j] = '\n';
          i++;
        } else if (tmpstr[i+1] == '\\') {
          tmpstr[j] = '\\';
          i++;
        } else {
          tmpstr[j] = tmpstr[i];
        }
      } else {
        tmpstr[j] = tmpstr[i];
      }
    }
    tmpstr[j] = '\0';
    printf(formated ? "\"%s\"" : "%s", tmpstr);
  } else {
    printf(formated ? "\"%s\"" : "%s", str);
  }
}

static void print_d_struct(drax_value value, int formated) {
  switch (DRAX_STYPEOF(value)) {
    case DS_LIST:
      print_list(CAST_LIST(value));
      break;

    case DS_FRAME:
      print_frame(CAST_FRAME(value));
      break;

    case DS_FUNCTION:
      printf("<function>");
      break;

    case DS_NATIVE:
      printf("<function:native>");
      break;

    case DS_MODULE:
      printf("<module>");
      break;

    case DS_STRING:
      print_string(CAST_STRING(value)->chars, formated);
      break;

    case DS_ERROR:
      printf("error");
      break;
  }
}

void drax_print_error(const char* format, va_list args) {
  vfprintf(stderr, format, args);
}

void print_drax(drax_value value, int formated) {
  if (IS_BOOL(value)) {
    printf(CAST_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    printf("nil");
  } else if (IS_NUMBER(value)) {
    printf("%g", CAST_NUMBER(value));
  } else if (IS_STRUCT(value)) {
    print_d_struct(value, formated);
  }
}

void dbreak_line() { putchar('\n'); }