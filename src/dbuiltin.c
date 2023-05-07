#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dbuiltin.h"
#include "ddefs.h"
#include "dtypes.h"
#include "dtime.h"
#include "dvm.h"

#include "mods/d_mod_os.h"

/* Make String as Return */
#define MSR(g, c)  \
    return DS_VAL(copy_dstring(g, c, strlen(c)));

#define DX_SUCESS_FN(v) *v = 1;

#define DX_ERROR_FN(v) *v = 0;

static drax_value __d_assert(d_vm* vm, int* stat) {
  drax_value b = pop(vm);
  drax_value a = pop(vm);

  if (!IS_BOOL(a)) {
    DX_SUCESS_FN(stat);
    return DRAX_TRUE_VAL;
  }

  if (CAST_BOOL(a)) {
     DX_SUCESS_FN(stat);
     return DRAX_TRUE_VAL;
  }
  
  DX_ERROR_FN(stat);

  if (!IS_STRING(b)) {
    return DS_VAL(new_derror(vm, (char*) "assert error"));
  }

  return DS_VAL(new_derror(vm, CAST_STRING(b)->chars));
}

static drax_value __d_sleep(d_vm* vm, int* stat) { 
  UNUSED(vm);
  drax_value val = pop(vm);

  if (!(IS_NUMBER(val))) {
    DX_ERROR_FN(stat);
    return DS_VAL(new_derror(vm, (char *) "Expected number as argument"));
  }
  
  double t = CAST_NUMBER(val);
  dx_sleep(t);
  DX_SUCESS_FN(stat);
  return DRAX_NIL_VAL;
}

static drax_value __d_typeof(d_vm* vm, int* stat) {
  DX_SUCESS_FN(stat);
  drax_value val = pop(vm);
  if (IS_STRUCT(val)) {
    switch (DRAX_STYPEOF(val)) {
      case DS_NATIVE:
      case DS_FUNCTION: MSR(vm, "function");
      case DS_STRING: MSR(vm, "string");
      case DS_LIST: MSR(vm, "list");
      case DS_FRAME: MSR(vm, "frame");
      case DS_MODULE: MSR(vm, "module");
      default: break;
    }
  }
  
  if (IS_BOOL(val)) { MSR(vm, "boolean"); }
  if (IS_NIL(val)) { MSR(vm, "nil"); }
  if (IS_NUMBER(val)) { MSR(vm, "number"); }

  DX_SUCESS_FN(stat);
  MSR(vm, "none");
}

/* Module OS */

static drax_value __d_get_env(d_vm* vm, int* stat) {
  drax_value a = pop(vm);

  if (!IS_STRING(a)) {
    DX_ERROR_FN(stat);
    return DS_VAL(new_derror(vm, (char *) "Expected string as argument"));
  }

  char* env = getenv(CAST_STRING(a)->chars);
  if (env == NULL) {
    DX_SUCESS_FN(stat);
    return DRAX_NIL_VAL;
  }

  DX_SUCESS_FN(stat);
  MSR(vm, env);
}

static drax_value __d_command(d_vm* vm, int* stat) {
  drax_value a = pop(vm);

  if (!IS_STRING(a)) {
    DX_ERROR_FN(stat);
    return DS_VAL(new_derror(vm, (char *) "Expected string as argument"));
  }

  char buf[4096];
  int bytes_read = d_command(CAST_STRING(a)->chars, buf, sizeof(buf));
  if (bytes_read == -1) {
    DX_ERROR_FN(stat);
    return DS_VAL(new_derror(vm, (char *) "Fail to execute command"));
  } else {
    buf[bytes_read] = '\0';
    char* r = replace_special_char('\n', 'n', buf);
    DX_SUCESS_FN(stat);
    MSR(vm, r);
  }
}

void load_callback_fn(d_vm* vm, vm_builtin_setter* reg) {
  reg(vm, "assert", 2, __d_assert);
  reg(vm, "typeof", 1, __d_typeof);
  reg(vm, "sleep", 1, __d_sleep);
}

void create_native_modules(d_vm* vm) {
  UNUSED(vm);
  drax_native_module* m;
  
  m = new_native_module(vm, "os", 4);
  const drax_native_module_helper os_helper[] = {
    {1, "command", __d_command },
    /*{1, "get_env", __d_get_env },*/
  };

  put_fun_on_module(m, os_helper, sizeof(os_helper) / sizeof(drax_native_module_helper));
  put_mod_table(vm->envs->modules, DS_VAL(m));

}
