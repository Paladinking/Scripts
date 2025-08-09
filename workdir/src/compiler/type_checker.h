#ifndef TYPE_CHECKER_H_00
#define TYPE_CHECKER_H_00
#include "tables.h" 

bool TypeChecker_run(Parser* parser);

bool TypeChecker_resolve_structs(Parser* parser);

#endif
