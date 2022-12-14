/* Drax Lang - 2022 */

#ifndef __DARCH_i386
#define __DARCH_i386

#ifdef __DRAX_BACKEND_ASM

#include "../dlowcode.h"

#define FL            "\n"
#define SPACE         " "

#define SD_GNU_STR       " \"%s\\n\""
#define DNUM_SIMPLE_REPR " %lu"
#define DNUM_ASM_REPR    " $%lu"
#define SD_GNU_ENT       "_start"

/* Syscalls */
#define SDSYSCALL     "int  $0x80"FL
#define SDCALL_EXIT   "mov $1, %%eax"
#define SDCALL_STDOUT "mov $1, %%ecx"
#define SDCODE_RETURN "xor %%ebx, %%ebx"
#define SDCALL_WRITE  "mov $4, %%eax"

#define GAS_ASCII     ".asciz"
#define GAS_LONG      ".long"

#define GAS_BSS       ".bss"
#define GAS_DATA      ".data"
#define GAS_TEXT      ".text"
#define GAS_GLOBAL    ".global"

#define ASMLD       "ld"
#define ASMCOMPILER "as "
#define ASM_AS_ARGS " --32 "
#define ASM_LD_ARGS " -m elf_i386 "

#define DMC(v)        v SPACE

int dx_init_bss_section();

int dx_init_data_section();

int dx_init_text_section();

int dx_init_exit();

char* get_ln_cmd(const char* name);

int get_asm_code(dline_cmd* v);

#endif
#endif
