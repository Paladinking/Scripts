#ifndef COMPILER_LOG_H_00
#define COMPILER_LOG_H_00
#include <stdint.h>
#include <stdbool.h>

enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
};

enum LogCatagory {
    // Status messages to the user.
    // LOG_LEVEL_INFO of this catagory uses stdout
    // All other logs got to stderr
    LOG_CATAGORY_USER,

    // Logs from name / type table
    LOG_CATAGORY_TABLES,
    // Logs from the tokenizer
    LOG_CATAGORY_TOKENIZER,
    // Logs from the scanner
    LOG_CATAGORY_SCANNER,
    // Logs from the parser
    LOG_CATAGORY_PARSER,
    // Logs from the type checker
    LOG_CATAGORY_TYPE_CHECKER,
    // Logs from the quads generator
    LOG_CATAGORY_QUADS_GENERATION,
    // Logs from the register allocator
    LOG_CATAGORY_REGISTER_ALLOCATION,
    // Logs from the assembly generator
    LOG_CATAGORY_ASM_GENERATION,
    // Logs from asembler 
    LOG_CATAGORY_ASSEMBLER,
    // Logs from the linker object parser
    LOG_CATAGORY_OBJECT_PARSER,
    // Logs from the linker
    LOG_CATAGORY_LINKER,
    // Logs from the linker object writer
    LOG_CATAGORY_OBJECT_WRITER,

    LOG_CATAGORY_MAX
};

#ifdef LOG_DISABLE
#define Log_Init()
#define Log_Shutdown()
#define Log_LogAtLevel(...)

#define LOG_USER_DEBUG(...)
#define LOG_USER_INFO(...)
#define LOG_USER_WARNING(...)
#define LOG_USER_ERROR(...)
#define LOG_USER_CRITICAL(...)

#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)
#define LOG_CRITICAL(...)

#define LOG_STR_DEBUG(str, len)
#define LOG_STR_INFO(str, len)
#define LOG_STR_WARNING(str, len)
#define LOG_STR_ERROR(str, len)
#define LOG_STR_CRITICAL(str, len)
#else
void Log_Init(bool log_to_socket);
void Log_Shutdown();
void Log_LogAtLevel(enum LogCatagory catagory, enum LogLevel level, 
                    const char* file, int32_t line, const char* fmt, ...);

void Log_LogStrAtLevel(enum LogCatagory catagory, enum LogLevel level,
                       const char* file, int32_t line,
                       const char* str, uint32_t len);

#define LOG_USER_DEBUG(...) Log_LogAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_USER_INFO(...) Log_LogAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_USER_WARNING(...) Log_LogAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_USER_ERROR(...) Log_LogAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_USER_CRITICAL(...) Log_LogAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_USER_STR_DEBUG(str, len) Log_LogStrAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_DEBUG, __FILE__, __LINE__, str, len)
#define LOG_USER_STR_INFO(str, len) Log_LogStrAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_INFO, __FILE__, __LINE__, str, len)
#define LOG_USER_STR_WARNING(str, len) Log_LogStrAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_WARNING, __FILE__, __LINE__, str, len)
#define LOG_USER_STR_ERROR(str, len) Log_LogStrAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_ERROR, __FILE__, __LINE__, str, len)
#define LOG_USER_STR_CRITICAL(str, len) Log_LogStrAtLevel(LOG_CATAGORY_USER, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, str, len)

#define LOG_DEBUG(...) Log_LogAtLevel(LOG_CATAGORY, LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Log_LogAtLevel(LOG_CATAGORY, LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) Log_LogAtLevel(LOG_CATAGORY, LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Log_LogAtLevel(LOG_CATAGORY, LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) Log_LogAtLevel(LOG_CATAGORY, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_STR_DEBUG(str, len) Log_LogStrAtLevel(LOG_CATAGORY, LOG_LEVEL_DEBUG, __FILE__, __LINE__, str, len)
#define LOG_STR_INFO(str, len) Log_LogStrAtLevel(LOG_CATAGORY, LOG_LEVEL_INFO, __FILE__, __LINE__, str, len)
#define LOG_STR_WARNING(str, len) Log_LogStrAtLevel(LOG_CATAGORY, LOG_LEVEL_WARNING, __FILE__, __LINE__, str, len)
#define LOG_STR_ERROR(str, len) Log_LogStrAtLevel(LOG_CATAGORY, LOG_LEVEL_ERROR, __FILE__, __LINE__, str, len)
#define LOG_STR_CRITICAL(str, len) Log_LogStrAtLevel(LOG_CATAGORY, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, str, len)
#endif


#endif
