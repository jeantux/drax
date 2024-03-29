#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ddefs.h"
#include "dvm.h"
#include "dbuiltin.h"
#include "dtypes.h"
#include "dgc.h"
#include "deval.h"

#include "dstring.h"
#include "dlist.h"

/* validation only number */
#define binary_op(vm, op) \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP); \
          return 1; \
        } \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, AS_VALUE(a op b)); 

#define back_scope(vm) \
        vm->envs->local->count = vm->envs->local->count - vm->active_instr->local_range; \
        vm->active_instr = callstack_pop(vm); \
        vm->ip = vm->active_instr->_ip;

#define process_lambda_function(vm, a, n, anf) \
    if (anf->arity != (int) a) { \
      raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a); \
      return 0; }\
    vm->active_instr->_ip = vm->ip;\
    callstack_push(vm, vm->active_instr);\
    zero_new_local_range(vm, anf->instructions->local_range);\
    vm->active_instr = anf->instructions;\
    vm->ip = anf->instructions->values;

#define STATUS_DCALL_ERROR            0
#define STATUS_DCALL_SUCCESS          1
#define STATUS_DCALL_SUCCESS_NO_CLEAR 2

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
 * Raise Helpers
*/

static void trace_error(d_vm* vm) {
  int last_line = -1;
  #define DO_TRACE_ERROR_ROUTINE() \
    d_instructions* instr = vm->active_instr; \
    if (instr) { \
      int idx = vm->ip - instr->values -1; \
      if (instr->lines[idx] != last_line) {\
      fprintf(stderr, TRACE_DESCRIPTION_LINE, instr->lines[idx]); \
      putchar('\n');} \
      last_line = instr->lines[idx]; \
    }

  while (CURR_CALLSTACK_SIZE(vm) > 0) {
    DO_TRACE_ERROR_ROUTINE();
    back_scope(vm);
  };
  DO_TRACE_ERROR_ROUTINE();
}

/* Delegate to drax_print_error */
void raise_drax_error(d_vm* vm, const char* format, ...) {
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

/**
 * clear new range of local variables
 * 
 * x                => used
 * 0                => not used
 * 
 * before:
 *  local->count    =>  (6)
 *  local->array    =>  |x|x|x|x|x|x|
 *  range           =>  (4)
 * 
 * after:
 *  local->array    =>  |x|x|x|x|x|x|0|0|0|0|
 *  local->count    =>  (10)
 * 
 */
static void zero_new_local_range(d_vm* vm, int range) {
  vm->envs->local->count += range;
  memset(&vm->envs->local->array[vm->envs->local->count - range], 0, sizeof(drax_value) * range);
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

#define return_if_not_found_error(vm, v, n, a) \
  if (v == 0) { \
    raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a); \
    return STATUS_DCALL_ERROR; \
  }

static int do_dcall_get_fun(d_vm* vm, char* n, int a, int global, drax_value* v ) {
  /**
   * Priority of search:
   * 
   *  1. Module  4. Native
   *  2. Local   5. Function
   *  3. Global
   */

  if (get_mod_table(vm->envs->modules, n, v) == 1) return 1;

  if (get_local_table(vm->envs->local, vm->active_instr->local_range, n, v) == 1) return 1;

  if (global) {
    if (get_var_table(vm->envs->global, n, v) == 1) return 1;
  }

  *v = get_fun_table(vm->envs->native, n, a);

  if (*v != 0) return 1;

  *v = get_fun_table(vm->envs->functions, n, a);

  return (*v != 0);
}

/**
 * Native Call Definitions
 * 
 * This function responds to the following status:
 * STATUS_DCALL_ERROR:   an error occurred
 * STATUS_DCALL_SUCCESS: success, proceed normally
 */
static int do_dcall_native(d_vm* vm, drax_value v) {
  int scs = 0;
  drax_os_native* ns = CAST_NATIVE(v);
  low_level_callback* nf = ns->function;
  drax_value result = nf(vm, &scs);

  if (!scs) {
    drax_error* e = CAST_ERROR(result);
    raise_drax_error(vm, e->chars);
    return STATUS_DCALL_ERROR;
  }

  push(vm, result);
  return STATUS_DCALL_SUCCESS;
}

/**
 * Execute the function call inside an element
 */
static int do_dcall_inside(d_vm* vm, char* n, int a, drax_value m) {
  drax_value v;

  if (IS_FRAME(m)) {
    if(get_value_dframe(CAST_FRAME(m), (char*) n, &v) == -1) {
      raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a);
      return STATUS_DCALL_ERROR;
    }

    drax_function* af = CAST_FUNCTION(v);
    process_lambda_function(vm, a, n, af)
    return STATUS_DCALL_SUCCESS_NO_CLEAR;
  }

  if (IS_MODULE(m)) {
    low_level_callback* nf = get_fun_on_module(CAST_MODULE(m), n, a);

    return_if_not_found_error(vm, nf, n, a);
    int scs = 0;
    drax_value result = nf(vm, &scs);

    if (!scs) {
      drax_error* e = CAST_ERROR(result);
      raise_drax_error(vm, e->chars);
      return STATUS_DCALL_ERROR;
    }

    push(vm, result);
    return STATUS_DCALL_SUCCESS;
  }
  
  if (IS_STRING(m)) {
    return dstr_handle_str_call(vm, n, a, m);
  }

  if (IS_LIST(m)) {
    return dlist_handle_call(vm, n, a, m);
  }

  return_if_not_found_error(vm, 0, n, a);
}

/**
 * Call Definitions
 * 
 * This function responds to the following status:
 * STATUS_DCALL_ERROR:            an error occurred
 * STATUS_DCALL_SUCCESS:          success, proceed normally
 * STATUS_DCALL_SUCCESS_NO_CLEAR: Proceed without clearing the stack
 */

static int do_dcall(d_vm* vm, int inside, int global, int pipe) {
  /* DEBUG( printf(" --do_dcall\n") ); */

  drax_value a = GET_VALUE(vm);

  /**
   * if call is using dot operator
   */
  drax_value m = 0;

  char* n = (char*) (pop(vm));

  if (inside) {
    if (pipe) {
      /**
       * if the function is called via pipe to a 
       * sub-element [module.function()], we invert
       * the argument with the base element.
       * 
       * the base element must be inverted to not be 
       * considered an argument.
       */

      drax_value _t = vm->stack[vm->stack_count - a];
      vm->stack[vm->stack_count - a] = vm->stack[vm->stack_count -1 - a];
      vm->stack[vm->stack_count -1 - a] = _t;
    }

    m = peek(vm, a);
  }

  if (inside) { return do_dcall_inside(vm, n, a, m); }

  drax_value v = 0;
  if(do_dcall_get_fun(vm, n, a, global, &v) == 0) {
    return_if_not_found_error(vm, 0, n, a);
  }

  if (IS_NATIVE(v)) { return do_dcall_native(vm, v); }

  drax_function* f = CAST_FUNCTION(v);

  process_lambda_function(vm, a, n, f);

  /**
   * In this case, the function name is 
   * only removed from the stack during the return,
   * because we can have nested calls.
   */

  return STATUS_DCALL_SUCCESS;
}

static int __start__(d_vm* vm, int inter_mode) {

  #define dbin_bool_op(op, v) \
      do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP); \
          return 1; \
        } \
        double b = CAST_NUMBER(pop(vm)); \
        double a = CAST_NUMBER(pop(vm)); \
        push(vm, v(a op b)); \
      } while (false)

  UNUSED(inter_mode);
  VMDispatch {
    VMcond(vm) {
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
      VMCase(OP_DSTR) {
        /* TODO: process db string */
        push(vm, GET_VALUE(vm));
        break;
      }
      VMCase(OP_CONST) {
        push(vm, GET_VALUE(vm));
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
        if(get_definition(vm, 0) == 0) { return 1; }

        break;
      }
      VMCase(OP_SET_L_ID) {
        drax_value v = pop(vm);
        char* k = (char*) GET_VALUE(vm);
        /* dgc_swap_locals(vm->envs->local); */
        put_local_table(vm->envs->local, k, v);
        break;
      }
      VMCase(OP_GET_L_ID) {
        if(get_definition(vm, 1) == 0) { return 1; }

        break;
      }
      VMCase(OP_SET_I_ID) {
        /**
         * This operation is invalid, we cannot change values 
         * of structures such as frames and lists, as this will 
         * influence old references, resulting in side effects.
         */

        raise_drax_error(vm, "error: invalid assigment\nuse helper functions to change values of structures\n");

        return 1;
      }
      VMCase(OP_GET_I_ID) {
        drax_value k = pop(vm);
        drax_value f = pop(vm);
        drax_value val;

        if(IS_STRING(f)) {
          if (dstr_handle_str_call(vm, (char*) k, 0, f) == 0) { return 1; };
          break;
        }

        if(IS_LIST(f)) {
          if (dlist_handle_call(vm, (char*) k, 0, f) == 0) { return 1; };
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

      /**
       * Internal references is only allowed to modules.
       * 
       * &module_name.function_name/arity
       */
      VMCase(OP_GET_REFI) {
        int a = (int) AS_NUMBER(pop(vm));
        char* n = (char*) GET_VALUE(vm);
        drax_value m = pop(vm);
        low_level_callback* nf = get_fun_on_module(CAST_MODULE(m), n, a);
        
        if (nf == 0) {
          raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a);
          return 1;
        }

        push(vm, DS_VAL(new_dllcallback(vm, nf, n, a)));
        break;
      }
      VMCase(OP_GET_REF) {
        int a = (int) AS_NUMBER(pop(vm));
        char* n = (char*) GET_VALUE(vm);

        drax_value v = get_fun_table(vm->envs->native, n, a);
        if (v == 0) {
          v = get_fun_table(vm->envs->functions, n, a);
        
          if (v == 0) {
            raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a);
            return 1;
          }
        }
        free(n);
        push(vm, v);        
        break;
      }
      VMCase(OP_EQUAL) {
        drax_value b = pop(vm);
        drax_value a = pop(vm);
        push(vm, BOOL_VAL(values_equal(a, b)));
        break;
      }
      VMCase(OP_GREATER) {
        dbin_bool_op(>, BOOL_VAL);
        break;
      }
      VMCase(OP_LESS) {
        dbin_bool_op(<, BOOL_VAL);
        break;
      }
      VMCase(OP_CONCAT) {
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
          drax_string* b = CAST_STRING(peek(vm, 0));
          drax_string* a = CAST_STRING(peek(vm, 1));

          int length = a->length + b->length;
          char* n_str = (char*) malloc(length + 1);
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
          raise_drax_error(vm, "Concat with unspected type");
          return 1;
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
          return 1;
        }
        
        push(vm, NUMBER_VAL(-CAST_NUMBER(pop(vm))));
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
      VMCase(OP_CALL_L) {
        if (do_dcall(vm, 0, 0, 0) == STATUS_DCALL_ERROR) return 1;
        break;
      }
      VMCase(OP_CALL_G) {
        if (do_dcall(vm, 0, 1, 0) == STATUS_DCALL_ERROR) return 1;
        break;
      }
      VMCase(OP_CALL_I) {
        int r = do_dcall(vm, 1, 0, 0);
        
        if (r == STATUS_DCALL_ERROR) return 1;
        if (r == STATUS_DCALL_SUCCESS_NO_CLEAR) break;
        
        drax_value t = pop(vm);

        /**
         * Remove the function name and struct 
         * from the stack.
         */
        pop(vm);
        push(vm, t);
        break;
      }
      VMCase(OP_CALL_IP) {
        int r = do_dcall(vm, 1, 0, 1);
        
        if (r == 0) return 1;
        if (r == 2) break;
        
        drax_value t = pop(vm);

        /**
         * Remove the function name and struct 
         * from the stack.
         */
        pop(vm);
        push(vm, t);
        break;
      }
      VMCase(OP_FUN) {
        drax_value v = GET_VALUE(vm);
        put_fun_table(vm->envs->functions, v);
        break;
      }
      VMCase(OP_AFUN) {
        drax_value v = GET_VALUE(vm);
        push(vm, v);
        break;
      }
      VMCase(OP_RETURN) {
        drax_value v = vm->active_instr->instr_count == 1 ?
          DRAX_NIL_VAL :
          pop(vm);
        back_scope(vm);
        push(vm, v);
        break;
      }
      VMCase(OP_EXIT) {
        /* check if is stopVM or exit */

        if (inter_mode) {
          if (peek(vm, 0) == 0) return 0;

          print_drax(pop(vm), inter_mode);
          dbreak_line();
        }
        dgc_swap(vm);
        return 0;
      }
      default: {
        printf("runtime error.\n");
        return 1;
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

int __run__(d_vm* vm, int inter_mode) {
  __init__(vm);
  return __start__(vm, inter_mode);
}
