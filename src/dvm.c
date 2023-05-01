#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "ddefs.h"
#include "dvm.h"
#include "dbuiltin.h"
#include "dtypes.h"
#include "dinspect.h"

#include "dstring.h"

/**
 * Helpers
*/

/* validation only number */
#define binary_op(vm, op) \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, AS_VALUE(a op b)); 

#define back_scope(vm) \
        vm->envs->local->count = vm->envs->local->count - vm->active_instr->local_range; \
        vm->active_instr = callstack_pop(vm); \
        vm->ip = vm->active_instr->_ip;

/* VM stack. */

void push(d_vm* vm, drax_value v) {
  vm->stack[vm->stack_count++] = v;
}

drax_value pop(d_vm* vm) {
  if (vm->stack_count == 0) return 0;
  vm->stack_count--;
  return vm->stack[vm->stack_count];
}

static drax_value pop_times(d_vm* vm, int t) {
  vm->stack_count -= t;
  return vm->stack[vm->stack_count];
}

static drax_value peek(d_vm* vm, int distance) {
  if (vm->stack_count == 0) return 0;
  return vm->stack[vm->stack_count -1 - distance];
}

/* VM Call stack */

static void callstack_push(d_vm* vm, d_instructions* v) {
  vm->call_stack->values[vm->call_stack->count++] = v;
}

static d_instructions* callstack_pop(d_vm* vm) {
  if (vm->call_stack->count == 0) return 0;
  vm->call_stack->count--;
  return vm->call_stack->values[vm->call_stack->count];
}

/**
 * Print Helpers
*/

static void print_drax(drax_value value, int formated);

static void dbreak_line() { putchar('\n'); }

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
      printf(formated ? "\"%s\"" : "%s", CAST_STRING(value)->chars);
      break;

    case DS_ERROR:
      printf("error");
      break;
  }
}

static void drax_print_error(const char* format, va_list args) {
  vfprintf(stderr, format, args);
}

static void print_drax(drax_value value, int formated) {
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

/**
 * Raise Helpers
*/

static void trace_error(d_vm* vm) {
  #define DO_TRACE_ERROR_ROUTINE() \
    d_instructions* instr = vm->active_instr; \
    if (instr) { \
      int idx = vm->ip - instr->values -1; \
      fprintf(stderr, TRACE_DESCRIPTION_LINE, instr->lines[idx]); \
      putchar('\n'); \
    }
  while (CURR_CALLSTACK_SIZE(vm) > 0) {
    DO_TRACE_ERROR_ROUTINE();
    back_scope(vm);
  };
  DO_TRACE_ERROR_ROUTINE();
}

/* Delegate to drax_print_error */
static void raise_drax_error(d_vm* vm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  drax_print_error(format, args);
  putchar('\n');
  va_end(args);

  trace_error(vm);
  __reset__(vm);
}

static bool values_equal(drax_value a, drax_value b) {
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    return CAST_NUMBER(a) == CAST_NUMBER(b);
  }

  if (IS_STRING(a) && IS_STRING(b)) {
    return CAST_STRING(a)->hash == CAST_STRING(b)->hash;
  }

  return a == b;
}

static int get_definition(d_vm* vm, int is_local) {
  #define _L_MSG_NOT_DEF "error: variable '%s' is not defined\n"
  char* k = (char*) GET_VALUE(vm);
  drax_value v;

  if (
    (get_mod_table(vm->envs->modules, k, &v) == 0) &&
    ((!is_local) || (get_local_table(vm->envs->local, vm->active_instr->local_range, k, &v) == 0))
  ) {
    if(get_var_table(vm->envs->global, k, &v) == 0) {
      raise_drax_error(vm, _L_MSG_NOT_DEF, k);
      return 0;
    }
  }

  push(vm, v);
  return 1;
}

/**
 * Call Definitions
 */

#define return_if_not_found_error(v, n, a) \
  if (v == 0) { \
  raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a); \
  return 0; \
}

#define return_if_native_call_error(s, r, c) \
  if (!s) { \
    drax_error* e = CAST_ERROR(r); \
    raise_drax_error(vm, c); \
    return 0; \
  }

static int do_dcall(d_vm* vm) {
  drax_value a = GET_VALUE(vm);
  drax_value m = peek(vm, a + 1);
  char* n = (char*) (peek(vm, a));

  if (IS_MODULE(m)) {
    low_level_callback* nf = get_fun_on_module(CAST_MODULE(m), n, a);

    return_if_not_found_error(nf, n, a);
    int scs = 0;
    drax_value result = nf(vm, &scs);
    return_if_native_call_error(scs, result, e->chars);

    push(vm, result);
    return 1;
  }
  
  if (IS_STRING(m)) {
    return dstr_handle_str_call(vm, n, a, m); /* TODO: Created error message */
  }

  drax_value v = get_fun_table(vm->envs->native, n, a);
  if (v == 0) {
    v = get_fun_table(vm->envs->functions, n, a);
  
    return_if_not_found_error(v, n, a);
  }

  if (IS_NATIVE(v)) {
    int scs = 0;
    drax_os_native* ns = CAST_NATIVE(v);
    low_level_callback* nf = ns->function;
    drax_value result = nf(vm, &scs);

    return_if_native_call_error(scs, result, e->chars);
    push(vm, result);
    return 1;
  }

  /* Drax Function */
  drax_function* f = CAST_FUNCTION(v);

  vm->active_instr->_ip = vm->ip;
  callstack_push(vm, vm->active_instr);
  vm->active_instr = f->instructions;
  vm->ip = f->instructions->values;

  return 1;
}

static void __start__(d_vm* vm, int inter_mode) {

  #define dbin_op(op, v) \
      do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP); \
          return; \
        } \
        double b = CAST_NUMBER(pop(vm)); \
        double a = CAST_NUMBER(pop(vm)); \
        push(vm, v(a op b)); \
      } while (false)

  UNUSED(inter_mode);
  VMDispatch {
    VMcond(vm) {
      VMCase(OP_CONST) {
        push(vm, GET_VALUE(vm));
        break;
      }
      VMCase(OP_NIL) {
        push(vm, DRAX_NIL_VAL);
        break;
      }
      VMCase(OP_TRUE) {
        push(vm, DRAX_TRUE_VAL);
        break;
      }
      VMCase(OP_FALSE) {
        push(vm, DRAX_FALSE_VAL);
        break;
      }
      VMCase(OP_LIST) {
        drax_value lc = pop(vm);
        int limit = (int) CAST_NUMBER(lc);
        drax_list* l = new_dlist(vm, limit);

        int i;
        for (i = 0; i < limit; i++) {
          put_value_dlist(l, peek(vm, (limit -1) - i));
        }

        pop_times(vm, limit);
        push(vm, DS_VAL(l));
        break;
      }
      VMCase(OP_FRAME) {
        drax_value lc = pop(vm);
        int limit = (int) CAST_NUMBER(lc);
        drax_frame* l = new_dframe(vm, limit);

        int i;
        for (i = limit; i > 0; i-=2) {
          char* k = (char*) peek(vm, i - 1);
          put_value_dframe(l, k, peek(vm, i - 2));
        }

        pop_times(vm, limit);
        push(vm, DS_VAL(l));
        break;
      }
      VMCase(OP_POP) {
        pop(vm);
        break;
      }
      VMCase(OP_PUSH) {
        drax_value v = GET_VALUE(vm);
        push(vm, v);
        break;
      }
      VMCase(OP_SET_G_ID) {
        drax_value v = pop(vm);
        char* k = (char*) GET_VALUE(vm);
        put_var_table(vm->envs->global, k, v);
        break;
      }
      VMCase(OP_GET_G_ID) {
        if(get_definition(vm, 0) == 0) { return; }

        break;
      }
      VMCase(OP_SET_L_ID) {
        drax_value v = pop(vm);
        char* k = (char*) GET_VALUE(vm);
        put_local_table(vm->envs->local, k, v);
        break;
      }
      VMCase(OP_GET_L_ID) {
        if(get_definition(vm, 1) == 0) { return; }

        break;
      }
      VMCase(OP_SET_I_ID) {
        drax_value n = pop(vm);
        drax_value k = pop(vm);
        drax_value f = pop(vm);
        put_value_dframe(CAST_FRAME(f), (char*) k, n);
        break;
      }
      VMCase(OP_GET_I_ID) {
        drax_value k = pop(vm);
        drax_value f = pop(vm);
        drax_value val;

        if(IS_STRING(f)) {
          if (dstr_handle_str_call(vm, (char*) k, 0, f) == 0) { return; }; /* TODO: set error */
          break;
        }

        if (IS_MODULE(f)) {
          /* return native function here */
          push(vm, DRAX_NIL_VAL);
          break;
        }

        if(get_value_dframe(CAST_FRAME(f), (char*) k, &val) == -1) {
          push(vm, DRAX_NIL_VAL);
          break;
        }

        push(vm, val);
        break;
      }
      VMCase(OP_EQUAL) {
        drax_value b = pop(vm);
        drax_value a = pop(vm);
        push(vm, BOOL_VAL(values_equal(a, b)));
        break;
      }
      VMCase(OP_GREATER) {
        dbin_op(>, BOOL_VAL);
        break;
      }
      VMCase(OP_LESS) {
        dbin_op(<, BOOL_VAL);
        break;
      }
      VMCase(OP_CONCAT) {
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
          drax_string* b = CAST_STRING(peek(vm, 0));
          drax_string* a = CAST_STRING(peek(vm, 1));

          int length = a->length + b->length;
          char* n_str = (char*) malloc(length);
          memcpy(n_str, a->chars, a->length);
          memcpy(n_str + a->length, b->chars, b->length);
          n_str[length] = '\0';

          drax_string* result = new_dstring(vm, n_str, length);
          pop_times(vm, 2);
          push(vm, DS_VAL(result));
        } else if (IS_LIST(peek(vm, 0)) && IS_LIST(peek(vm, 1))) {
          drax_list* b = CAST_LIST(peek(vm, 0));
          drax_list* a = CAST_LIST(peek(vm, 1));
          int length = a->length + b->length;
          drax_list* result = new_dlist(vm, length);
          memcpy(result->elems, a->elems, a->length * sizeof(drax_value));
          memcpy(result->elems + a->length, b->elems, b->length * sizeof(drax_value));

          result->length = length;
          pop_times(vm, 2);
          push(vm, DS_VAL(result));
        } else{
          raise_drax_error(vm, "Unspected type");
          return;
        }
        break;
      }
      VMCase(OP_ADD) {
        binary_op(vm, +);
        break;
      }
      VMCase(OP_SUB) {
        binary_op(vm, -);
        break;
      }
      VMCase(OP_MUL) {
        binary_op(vm, *);
        break;
      }
      VMCase(OP_DIV) {
        binary_op(vm, /);
        break;
      }
      VMCase(OP_NOT) {
        drax_value v = pop(vm);
        push(vm, BOOL_VAL(IS_FALSE(v)));
        break;
      }
      VMCase(OP_NEG) {
        if (!IS_NUMBER(peek(vm, 0))) {
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP);
          return;
        }
        
        push(vm, NUMBER_VAL(-CAST_NUMBER(pop(vm))));
        break;
      }
      VMCase(OP_PRINT) {
        print_drax(pop(vm), 0);
        dbreak_line();
        break;
      }
      VMCase(OP_JMP) {
        uint16_t offset = dg16_byte(vm);

        vm->ip += offset;
        break;
      }
      VMCase(OP_JMF) {
        uint16_t offset = dg16_byte(vm);
        if (IS_FALSE(peek(vm, 0))) vm->ip += offset;
        break;
      }
      VMCase(OP_LOOP) {
        break;
      }
      VMCase(OP_CALL) {
        if (do_dcall(vm) == 0) return;
        
        break;
      }
      VMCase(OP_FUN) {
        drax_value v = GET_VALUE(vm);
        put_fun_table(vm->envs->functions, v);
        break;
      }
      VMCase(OP_RETURN) {
        drax_value v = vm->active_instr->instr_count == 1 ?
          DRAX_NIL_VAL :
          pop(vm);
        back_scope(vm);
        pop(vm); /* func. name */
        push(vm, v);
        break;
      }
      VMCase(OP_EXIT) {
        /* check if is stopVM or exit */

        if (inter_mode) {
          if (peek(vm, 0) == 0) return;

          print_drax(pop(vm), inter_mode);
          dbreak_line();
        }
        return;
      }
      default: {
        printf("runtime error.\n");
        return;
      }
    }
  }
  
}

/* Constructor */

static void register_builtin(d_vm* vm, const char* n, int a, low_level_callback* f) {
  drax_value v = DS_VAL(new_dllcallback(vm, f, n, a));
  put_fun_table(vm->envs->native, v);
}

static void initialize_builtin_functions(d_vm* vm) {
  load_callback_fn(vm, &register_builtin);
}

static dt_envs* new_environment() {
  dt_envs* e = (dt_envs*) malloc(sizeof(dt_envs));
  e->global = new_var_table();
  e->functions = new_fun_table();
  e->native = new_fun_table();
  e->local = new_local_table();
  e->modules = new_mod_table();
  return e;
}

/**
 * Create a new drax vm
*/

static void __init__(d_vm* vm) {
  /**
   * vm->active_instr->values: Must always point to the first statement.
   *  - Used by stack trace.
   *  - Used to initiate a function call.
   */

  vm->ip = vm->active_instr->values;
}

void __reset__(d_vm* vm) {
  vm->active_instr = NULL;
  
  free(vm->instructions->values);
  free(vm->instructions->lines);
  free(vm->instructions);
  
  vm->instructions = new_instructions();

  vm->call_stack->count = 0;
  vm->stack_count = 0;
  vm->ip = NULL;
}

d_vm* createVM() {
  d_vm* vm = (d_vm*) malloc(sizeof(d_vm));
  vm->instructions = new_instructions();
  vm->d_ls = NULL;

  vm->stack = (drax_value*) malloc(sizeof(drax_value) * MAX_STACK_SIZE);
  vm->stack_size = MAX_STACK_SIZE;
  vm->stack_count = 0;
  vm->envs = new_environment();

  /**
   * Created on builtin definitions
  */
  create_native_modules(vm);

  initialize_builtin_functions(vm);

  vm->call_stack = (dcall_stack*) malloc(sizeof(dcall_stack));
  vm->call_stack->size = CALL_STACK_SIZE;
  vm->call_stack->count = 0;
  vm->call_stack->values = (d_instructions**) malloc(sizeof(d_instructions*) * CALL_STACK_SIZE);

  return vm;
}

void __run__(d_vm* vm, int inter_mode) {
 __init__(vm);
 __start__(vm, inter_mode);
}