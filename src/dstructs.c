#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dstructs.h"
#include "dhandler.h"
#include "dtypes.h"
#include "ddefs.h"
#include "dvm.h"

#define ALLOCATE_DSTRUCT(b, type, d_struct_type) \
    (type*) allocate_struct(b, sizeof(type), d_struct_type)

static d_struct* allocate_struct(d_vm* vm, size_t size, dstruct_type type) {
  d_struct* d = (d_struct*) malloc(sizeof(d_struct) * size);
  d->type = type;
  d->checked = 0;
  d->next = vm->d_ls;
  vm->d_ls = d;

  return d;
}

/* 
 * Drax list helpers
 */

#define LIST_PRE_SIZE 10

drax_list* new_dlist(d_vm* vm, int cap) {
  drax_list* l = ALLOCATE_DSTRUCT(vm, drax_list, DS_LIST);
  l->length = 0;
  l->cap = cap == 0 ? LIST_PRE_SIZE : cap;
  l->elems = malloc(sizeof(drax_value) * l->cap);
  return l;
}

void put_value_dlist(drax_list* l, drax_value v) {
  if (l->cap <= l->length) {
    l->cap = (l->cap + LIST_PRE_SIZE);
    l->elems = realloc(l->elems, sizeof(drax_value) * l->cap);
  }

  l->elems[l->length] = v;
  l->length++;
}

drax_error* new_derror(d_vm* vm, char* msg) {
  drax_error* v = ALLOCATE_DSTRUCT(vm, drax_error, DS_FUNCTION);
  v->chars = msg;
  v->length = strlen(msg);
  return v;
}

drax_os_native* new_dllcallback(d_vm* vm, low_level_callback* f, const char* name, int arity) {
  drax_os_native* native = ALLOCATE_DSTRUCT(vm, drax_os_native, DS_NATIVE);
  native->name = name;
  native->function = f;
  native->arity = arity;
  return native;
}

drax_function* new_function(d_vm* vm) {
  drax_function* f = ALLOCATE_DSTRUCT(vm, drax_function, DS_FUNCTION);
  f->arity = 0;
  f->instructions = new_instructions();

  return f;
}

static drax_string* allocate_string(d_vm* vm, char* chars, int length, uint32_t hash) {
  drax_string* string = ALLOCATE_DSTRUCT(vm, drax_string, DS_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  return string;
}

drax_string* new_dstring(d_vm* vm, char* chars, int length) {
  uint32_t hash = fnv1a_hash(chars, length);

  return allocate_string(vm, chars, length, hash);
}

drax_string* copy_dstring(d_vm* vm, const char* chars, int length) {
  char* new_chars = malloc(sizeof(char) * length + 1);
  int new_length = 0;
  int i;
  for (i = 0; i < length; i++) {
    if (chars[i] == '\\') {
      if(i + 1 < length && chars[i+1] == '\"') {
        new_chars[new_length++] = '\"';
        i++;
        continue;
      } 
      
      if(i + 1 < length && chars[i+1] == '\\') {
        new_chars[new_length++] = '\\';
        i++;
        continue;
      }
    }
    new_chars[new_length++] = chars[i];
  }
  new_chars[new_length] = '\0';

  uint32_t hash = fnv1a_hash(new_chars, new_length);

  return allocate_string(vm, new_chars, new_length, hash);
}

drax_frame* new_dframe(d_vm* vm, int cap) {
  drax_frame* l = ALLOCATE_DSTRUCT(vm, drax_frame, DS_FRAME);
  l->length = 0;
  l->cap = cap == 0 ? 8 : cap;
  l->values = (drax_value*) malloc(sizeof(drax_value) * l->cap);
  l->literals = (char**) malloc(sizeof(char*) * l->cap);
  l->keys = (int*) malloc(sizeof(int) * l->cap);
  return l;
}

/**
 * return the index of the value in the frame
 * -1 if not found
 */

int get_value_dframe(drax_frame* l, char* name, drax_value* value) {
  int i;
  int k = fnv1a_hash(name, strlen(name));
  for (i = 0; i < l->length; i++) {
    if (l->keys[i] == k) {
      *value = l->values[i];
      return i;
    }
  }
  return -1;
}

void put_value_dframe(drax_frame* l, char* k, drax_value v) {
  if (l->cap <= l->length) {
    l->cap = (l->cap + 8);
    l->keys = realloc(l->keys, sizeof(int) * l->cap);
    l->values = realloc(l->values, sizeof(drax_value) * l->cap);
    l->literals = realloc(l->literals, sizeof(char*) * l->cap);
  }

  drax_value tv;
  int idx = get_value_dframe(l, k, &tv);

  if (idx != -1) {
    l->values[idx] = v;
    return;
  }

  l->keys[l->length] = fnv1a_hash(k, strlen(k));
  l->literals[l->length] = k;
  l->values[l->length] = v;
  l->length++;
}

/**
 * Module helpers
 */

drax_native_module* new_native_module(d_vm* vm, const char* name, int cap) {
  drax_native_module* m = ALLOCATE_DSTRUCT(vm, drax_native_module, DS_MODULE);
  m->name = name;
  m->count = 0;
  m->arity = (int*) malloc(sizeof(int) * cap);
  m->fun = (low_level_callback**) malloc(sizeof(low_level_callback*) * cap);
  m->fn_names = (const char**) malloc(sizeof(const char*) * cap);
  return m;
}

void put_fun_on_module(drax_native_module* m, const drax_native_module_helper helper[], int size) {
  int i;
  for (i = 0; i < size; i++) {
    m->fn_names[i] = helper[i].name;
    m->fun[i] = helper[i].fun;
    m->arity[i] = helper[i].arity;
  }
  m->count = size;
}

low_level_callback* get_fun_on_module(drax_native_module* m, const char* n, int a) {
  int i;
  for (i = 0; i < m->count; i++) {
    if (strcmp(m->fn_names[i], n) == 0) {
      if (m->arity[i] != a) { continue; }
      return m->fun[i];
    }
  }
  return 0;
}
