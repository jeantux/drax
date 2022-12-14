#ifdef __DRAX_BACKEND_ASM

/* Drax Lang - 2022 */
#include <string.h>
#include <stdlib.h>

#include "i386.h"
#include "../dlowcode.h"
#include "../dgen.h"
#include "../ddefs.h"

static const char* dxasm_reg_table(dlcode_register t) {
  switch (t) {
    case DRG_RX0: return DMC("%%eax");
    case DRG_RX1: return DMC("%%ebx");
    case DRG_RX2: return DMC("%%ecx");
    case DRG_RX3: return DMC("%%edx");
    case DRG_RX4: return DMC("%%esi");
    case DRG_RX5: return DMC("%%edi");
    case DRG_RX6: return DMC("%%esp");
    case DRG_RX7: return DMC("%%ebp");

    default: return "";
  }
}

static const char* dxasm_cmd_table(dlcode_op t) {
  switch (t) {
    case DOP_ADD:    return DMC("add");
    case DOP_SUB:    return DMC("sub");
    case DOP_MUL:    return DMC("imul");
    case DOP_DIV:    return DMC("idiv"); /* Invalid */
    case DOP_MOV:    return DMC("mov");
    case DOP_PUSH:   return DMC("push");
    case DOP_POP:    return DMC("pop");
    case DOP_CALL:   return DMC("call");
    case DOP_JUMP:   return DMC("jmp");
    case DOP_RETURN: return DMC("ret");
    case DOP_LCOMM:  return DMC(".lcomm");
    case DOP_COMM:   return DMC(".comm");

    default: return "";
  }
}

/* common signatures */

  int dx_init_bss_section() {
    return df_asm_gen(GAS_BSS FL);
  }

  int dx_init_data_section() {
    return df_asm_gen(GAS_DATA FL);
  }

  int dx_init_text_section() {
    return df_asm_gen(GAS_TEXT FL);
  }

  int dx_init_exit() {
    return df_asm_gen(
    // SDCODE_RETURN
      SDCALL_EXIT FL
      SDSYSCALL   FL
    );
  }

char* get_ln_cmd(const char* name) {
  const char* fcmd = ASMLD ASM_LD_ARGS " -s "ASMFO0 " "ASMLIBS " -o ";
  int sz = strlen(fcmd) + strlen(name) + 1;
  char* cmd = (char*) calloc(sizeof(char), sz);

  strcat(cmd, fcmd);
  strcat(cmd, name);
  cmd[sz] = '\0';
  return cmd;
}

/* end of signatures */

static int call_exit(dline_cmd* e) {
  if (DRG_NONE == e->rg0) {
    df_asm_gen(SDCODE_RETURN FL);
  } else {
    df_asm_gen(dxasm_cmd_table(DOP_MOV));
    df_asm_gen(dxasm_reg_table(e->rg0));
    df_asm_gen(",%%ebx" FL); // Int error code
  }
  df_asm_gen(dxasm_cmd_table(DOP_JUMP));
  df_asm_gen("exit");
  return df_asm_gen(FL);
}

static int dx_i386_puts(dline_cmd* e) {
  df_asm_gen("mov $%s, %%ecx" FL, CAST_STRING(e->value));
  df_asm_gen("call dputs\n");

  return 0;
}

static int dx_i386_const_string(dline_cmd* e) {
  return df_asm_gen(GAS_ASCII SD_GNU_STR FL, CAST_STRING(e->value));
}

static int dx_i386_const_int(dline_cmd* e) {
  return df_asm_gen(GAS_LONG DNUM_SIMPLE_REPR FL, CAST_INT(e->value));
}

static int dx_label(dline_cmd* e) {
  return df_asm_gen("%s: "FL, CAST_STRING(e->value));
}

static int dx_i386_mov(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));
  
  if (e->rg_qtt == 1) {
    char* var;
    asprintf(&var, DNUM_ASM_REPR, e->value);
    df_asm_gen(var);
  } else {
    df_asm_gen(dxasm_reg_table(e->rg1));
  }

  df_asm_gen(",");
  df_asm_gen(dxasm_reg_table(e->rg0));

  df_asm_gen(FL);

  return 0;
}

static int dx_i386_arith(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));

  if (e->op != DOP_DIV) {
    if (e->rg_qtt == 1) {
      char* var;
      asprintf(&var, DNUM_ASM_REPR, e->value);
      df_asm_gen(var);
    } else {
      df_asm_gen(dxasm_reg_table(e->rg0));
    }
    df_asm_gen(",");
  }

  df_asm_gen(dxasm_reg_table(e->rg_qtt == 1 ? e->rg0 : e->rg1));
  df_asm_gen(FL);

  return 0;
}

static int dx_i386_push(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));
  df_asm_gen(dxasm_reg_table(e->rg0));
  df_asm_gen(FL);

  return 0;
}

static int dx_i386_pop(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));
  df_asm_gen(dxasm_reg_table(e->rg0));
  df_asm_gen(FL);

  return 0;
}

static int dx_i386_global(dline_cmd* e) {
  df_asm_gen(GAS_GLOBAL SPACE);
  df_asm_gen((char *) e->value);
  df_asm_gen(FL);

  return 0;
}

static int dx_i386_return(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));
  df_asm_gen(FL);

  return 0;
}

static int dx_i386_jump(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));
  df_asm_gen(CAST_STRING(e->value));
  df_asm_gen(FL);

  return 0;
}

static int dx_type_def(dline_cmd* e) {
  df_asm_gen(dxasm_cmd_table(e->op));

  d_byte_pair_def* dpair = CAST_DRAX_PAIR(e->value);

  df_asm_gen(CAST_STRING(dpair->left));
  df_asm_gen(",");
  df_asm_gen(CAST_STRING(dpair->right)); /* size */
  df_asm_gen(FL);

  return 0;
}

int get_asm_code(dline_cmd* v) {
  switch (v->op) {
    case DOP_MRK_ID:
    case DOP_LABEL: return dx_label(v);
    case DOP_MOV: return dx_i386_mov(v);
    
    /* Arith section */
    case DOP_ADD:
    case DOP_SUB:
    case DOP_MUL:
    case DOP_DIV: return dx_i386_arith(v);
    
    /* Jump Section */
    case DOP_CALL:
    case DOP_JUMP: return dx_i386_jump(v);
    
    /* General Section */
    case DOP_GLOBAL: return dx_i386_global(v); // global, label
    case DOP_PUSH: return dx_i386_push(v);
    case DOP_POP: return dx_i386_pop(v);
    case DOP_EXIT: return call_exit(v);
    case DOP_STRING: return dx_i386_const_string(v);
    case DOP_INT: return dx_i386_const_int(v);
    case DOP_PUTS: return dx_i386_puts(v);
    case DOP_RETURN: return dx_i386_return(v);
    
    /* types def. */
    case DOP_LCOMM:
    case DOP_COMM: return dx_type_def(v);
  
    default: return 0;
  }

  return 0;
}

#endif