#ifndef SUBPROCESS_H_
#define SUBPROCESS_H_
#include "dynamic_string.h"

// Run cmd as a subprocess, wait for it to complete, and save stdout to out
// buffer.
bool subprocess_run(const wchar_t *cmd, String *out, unsigned timeout_millies,
                    unsigned long *exit_code);

#endif // SUBPROCESS_H_
