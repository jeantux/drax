/* drax Lang - 2022
 * Jean Carlos (jeantux)
 */

#ifndef __DFLAGS
#define __DFLAGS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dtypes.h"

#define drax_VM_VERSION  "v0.0.1-dev"
#define drax_LIB_VERSION "LC-0.0.0-dev"

void initial_info();

bimode get_bimode(int argc, char** argv);

#endif
