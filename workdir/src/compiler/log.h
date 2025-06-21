#ifndef COMPILER_LOG_H_00
#define COMPILER_LOG_H_00
#include <stdint.h>

enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
};

#ifdef LOG_DISABLE
#define Log_Init()
#define Log_Shutdown()
#define Log_LogAtLevel(...)

#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)
#define LOG_CRITICAL(...)
#else
void Log_Init();
void Log_Shutdown();
void Log_LogAtLevel(enum LogLevel level, const char* file, int32_t line, const char* fmt, ...);

#define LOG_DEBUG(...) Log_LogAtLevel(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Log_LogAtLevel(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) Log_LogAtLevel(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Log_LogAtLevel(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) Log_LogAtLevel(LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)
#endif


#endif
