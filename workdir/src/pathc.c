#define UNICODE
#ifdef __GNUC__
#define main entry_main
#endif
#include "args.h"
#include "path_utils.h"
#include "printf.h"
#include <tlhelp32.h>
#include <windows.h>

DWORD GetParentProcessId() {
    // Get the current process ID
    DWORD currentProcessId = GetCurrentProcessId();

    // Create a snapshot of the system processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Iterate through the processes to find the parent process
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentProcessId) {
                // Found the current process in the snapshot, return its parent
                // process ID
                CloseHandle(hSnapshot);
                return pe32.th32ParentProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    // Process not found or unable to retrieve information
    CloseHandle(hSnapshot);
    return 0;
}

OpStatus paths_add(int argc, LPWSTR *argv, PathBuffer *path_buffer, BOOL before,
                   BOOL expand) {

    if (argc < 2) {
        return OP_MISSSING_ARG;
    }
    OpStatus status = OP_NO_CHANGE;

    for (int arg = 1; arg < argc; ++arg) {
        OpStatus res = path_add(argv[arg], path_buffer, before, expand);
        if (res == OP_SUCCESS) {
            status = OP_SUCCESS;
        } else if (res != OP_NO_CHANGE) {
            return res;
        }
    }
    return status;
}

OpStatus paths_remove(int argc, LPWSTR *argv, PathBuffer *path_buffer,
                      BOOL before, BOOL expand) {
    if (argc < 2) {
        return OP_MISSSING_ARG;
    }
    if (before) {
        return OP_INVALID_PATH;
    }
    BOOL removed_any = FALSE;
    for (int arg = 1; arg < argc; ++arg) {
        OpStatus status = path_remove(argv[arg], path_buffer, expand);
        if (status == OP_SUCCESS) {
            removed_any = TRUE;
        } else if (status != OP_NO_CHANGE) {
            return status;
        }
    }
    if (!removed_any) {
        return OP_NO_CHANGE;
    }
    return OP_SUCCESS;
}

typedef enum _OperationType {
    OPERATION_ADD,
    OPERATION_REMOVE,
    OPERATION_MOVE,
    OPERATION_PRUNE
} OperationType;

PathBuffer path_buffer = {NULL, 0, 0};

#define USAGE                                                                  \
    "usage: pathc [-h | --help] [-n | --no-expand] [-u | --update] [-b | "     \
    "--before] [-v | --variable <var>] <command> [<args>]\n"

const char *help_message = USAGE
    "\n"
    "Adds or removes a file path to PATH. By default, paths are added at the "
    "end of PATH, and the new PATH is printed to stdout.\n"
    "Does not add paths that already exists in PATH.\n\n"
    "The following commands are available:\n\n"
    "  add, a                Add all filepaths in <args> to PATH\n"
    "  remove, r             Remove all filepaths in <args> from PATH\n"
    "  move, m               Move all filepaths in <args> as if using remove "
    "followed by add\n"
    "  prune, p              Remove all duplicate or invalid filepaths in "
    "PATH\n\n"
    "The following flags can be used:\n\n"
    "  -h, --help            Displays this help message\n"
    "  -n, --no-expand       Do not expand relative paths to an absolute path "
    "before adding it\n"
    "  -u, --update          Apply the new PATH to the parent process "
    "environment instead of printing it\n"
    "  -b, --before          Add paths to the beginning of PATH\n"
    "  -v, --variable <var>  Modify environment variable <var> instead of PATH";

int pathc(int argc, LPWSTR *argv, HANDLE out, HANDLE err) {
    if (find_flag(argv, &argc, L"-h", L"--help")) {
        _printf("%s\n", help_message);
        return 0;
    }

    if (argc < 2) {
        _printf_h(err, "Missing operation type\n" USAGE);
        return 2;
    }

    BOOL expand = !find_flag(argv, &argc, L"-n", L"--no-expand");
    BOOL before = find_flag(argv, &argc, L"-b", L"--before");
    BOOL update = find_flag(argv, &argc, L"-u", L"--update");

    LPWSTR var;
    int found = find_flag_value(argv, &argc, L"-v", L"--variable", &var);
    if (found == -1) {
        _printf_h(err, "Missing variable value\n");
        return 2;
    } else if (!found) {
        var = L"PATH";
    }

    OperationType operation;
    if (wcscmp(argv[1], L"a") == 0 || wcscmp(argv[1], L"add") == 0) {
        operation = OPERATION_ADD;
    } else if (wcscmp(argv[1], L"r") == 0 || wcscmp(argv[1], L"remove") == 0) {
        operation = OPERATION_REMOVE;
    } else if (wcscmp(argv[1], L"m") == 0 || wcscmp(argv[1], L"move") == 0) {
        operation = OPERATION_MOVE;
    } else if (wcscmp(argv[1], L"p") == 0 || wcscmp(argv[1], L"prune") == 0) {
        operation = OPERATION_PRUNE;
    } else {
        _printf_h(err, "Invalid operation type\n");
        return 2;
    }

    HANDLE heap = GetProcessHeap();

    path_buffer.capacity = 2000;
    if (!get_envvar(var, 20 * (argc - 1), &path_buffer)) {
        _printf_h(err, "Could not get %s\n", var);
        return 6;
    }

    OpStatus status = OP_NO_CHANGE;

    if (operation == OPERATION_ADD) {
        status = paths_add(argc - 1, argv + 1, &path_buffer, before, expand);
    } else if (operation == OPERATION_REMOVE) {
        status = paths_remove(argc - 1, argv + 1, &path_buffer, before, expand);
    } else if (operation == OPERATION_MOVE) {
        status = paths_remove(argc - 1, argv + 1, &path_buffer, FALSE, expand);
        if (status <= OP_NO_CHANGE) {
            status =
                paths_add(argc - 1, argv + 1, &path_buffer, before, expand);
        }
    } else if (operation == OPERATION_PRUNE) {
        status = path_prune(&path_buffer);
    }
    if (status == OP_MISSSING_ARG) {
        _printf_h(err, "Missing argument\n");
        return 2;
    }
    if (status == OP_INVALID_PATH) {
        _printf_h(err, "Invalid argument\n");
        return 2;
    }
    if (status == OP_OUT_OF_MEMORY) {
        _printf_h(err, "Out of memory\n");
        return 3;
    }
    if (update) {
        if (status == OP_NO_CHANGE) {
            return 1;
        }
        DWORD id = GetParentProcessId();
        if (id == 0) {
            _printf_h(err, "Could not get parent\n");
            return 4;
        }
        int res = SetProcessEnvironmentVariable(var, path_buffer.buffer, id);
        if (res != 0) {
            _printf_h(err, "Failed setting %s\n", var);
            return 5;
        }
    } else {
        _printf_h(out, "%.*S\n", path_buffer.length, path_buffer.buffer);
    }

    if (status == OP_NO_CHANGE) {
        return 1;
    }
    return 0;
}

int main() {
    HANDLE heap = GetProcessHeap();
    LPWSTR args = GetCommandLine();
    int argc;
    LPWSTR *argv = parse_command_line_with(args, &argc, FALSE, TRUE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    int status = pathc(argc, argv, out, err);
    FlushFileBuffers(out);
    FlushFileBuffers(err);
    HeapFree(heap, 0, expanded);
    WString_free(&path_buffer);
    HeapFree(heap, 0, argv);
    ExitProcess(status);
    return status;
}
