/* drax Lang - 2023
 * Jean Carlos (jeantux)
 */

 
#ifndef __D_MOD_STRING
#define __D_MOD_STRING
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "dvm.h"
#include "dstructs.h"

#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L

char* strndup(const char *s, size_t n);

#endif

char* str_format_output(const char* str);

int dstr_handle_str_call(d_vm* vm, char* n, int a, drax_value o);

#endif