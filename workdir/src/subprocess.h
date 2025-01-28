#ifndef SUBPROCESS_H_
#define SUBPROCESS_H_
#include "dynamic_string.h"


#define SUBPROCESS_STDERR_NONE 1
#define SUBPROCESS_STDIN_DEVNULL 2

// Run cmd as a subprocess, wait for it to complete, and save stdout to out
// buffer.
bool subprocess_run(const wchar_t *cmd, String *out, unsigned timeout_millies,
                    unsigned long *exit_code, unsigned opts);

#endif // SUBPROCESS_H_
