/**
 * @file log.h
 *
 */

#ifndef G_LOG_H
#define G_LOG_H

#include <stdio.h>
#include <time.h>

// Define log levels
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_FATAL   1
#define LOG_LEVEL_ERROR   2
#define LOG_LEVEL_WARN    3
#define LOG_LEVEL_INFO    4
#define LOG_LEVEL_DEBUG   5
#define LOG_LEVEL_TRACE   6

/* Global default log level */
#ifndef GLOBAL_LOG_LEVEL
#define GLOBAL_LOG_LEVEL LOG_LEVEL_INFO
#endif

/* Per-file log level override: uses global level if not defined by the file */
#ifndef LOG_LEVEL
#define LOG_LEVEL GLOBAL_LOG_LEVEL
#endif

// Color macros for terminal output (optional)
#define COLOR_RESET     "\033[0m"

// Regular Colors
#define COLOR_BLACK     "\033[30m"
#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_BLUE      "\033[34m"
#define COLOR_MAGENTA   "\033[35m"
#define COLOR_CYAN      "\033[36m"
#define COLOR_WHITE     "\033[37m"

// Bright Colors
#define COLOR_BBLACK    "\033[90m"
#define COLOR_BRED      "\033[91m"
#define COLOR_BGREEN    "\033[92m"
#define COLOR_BYELLOW   "\033[93m"
#define COLOR_BBLUE     "\033[94m"
#define COLOR_BMAGENTA  "\033[95m"
#define COLOR_BCYAN     "\033[96m"
#define COLOR_BWHITE    "\033[97m"

// Background Colors
#define BG_BLACK        "\033[40m"
#define BG_RED          "\033[41m"
#define BG_GREEN        "\033[42m"
#define BG_YELLOW       "\033[43m"
#define BG_BLUE         "\033[44m"
#define BG_MAGENTA      "\033[45m"
#define BG_CYAN         "\033[46m"
#define BG_WHITE        "\033[47m"

// Bright Backgrounds
#define BG_BBLACK       "\033[100m"
#define BG_BRED         "\033[101m"
#define BG_BGREEN       "\033[102m"
#define BG_BYELLOW      "\033[103m"
#define BG_BBLUE        "\033[104m"
#define BG_BMAGENTA     "\033[105m"
#define BG_BCYAN        "\033[106m"
#define BG_BWHITE       "\033[107m"

// Text styles
#define STYLE_BOLD      "\033[1m"
#define STYLE_UNDERLINE "\033[4m"
#define STYLE_REVERSED  "\033[7m"

static inline const char* _log_timestamp() {
    static char buffer[32];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
    return buffer;
}

// Logging macros
#if LOG_LEVEL >= LOG_LEVEL_FATAL
#define LOG_FATAL(fmt, ...) \
    fprintf(stderr, COLOR_RED "[FATAL] [%s] [%s:%d:%s] \n\t" fmt COLOR_RESET "\n", \
            _log_timestamp(), __FILE__, __LINE__, __func__, ##__VA_ARGS__) 
#else
#define LOG_FATAL(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, COLOR_MAGENTA "[ERROR] [%s] [%s:%d:%s] \n\t" fmt COLOR_RESET "\n", \
            _log_timestamp(), __FILE__, __LINE__, __func__, ##__VA_ARGS__) 
#else
#define LOG_ERROR(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...) \
    fprintf(stderr, COLOR_YELLOW "[WARN] [%s] [%s:%d:%s] \n\t" fmt COLOR_RESET "\n", \
            _log_timestamp(), __FILE__, __LINE__, __func__, ##__VA_ARGS__) 
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) \
    fprintf(stdout, COLOR_GREEN "[INFO] [%s] [%s] " fmt COLOR_RESET "\n", \
            _log_timestamp(), __func__, ##__VA_ARGS__) 
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) \
    fprintf(stdout, COLOR_CYAN "[DEBUG] [%s] [%s:%d:%s] \n\t" fmt COLOR_RESET "\n", \
            _log_timestamp(), __FILE__, __LINE__, __func__, ##__VA_ARGS__) 
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_TRACE
#define LOG_TRACE(fmt, ...) \
    fprintf(stdout, COLOR_BLUE "[TRACE] [%s] [%s] " fmt COLOR_RESET "\n", \
            _log_timestamp(), __func__, ##__VA_ARGS__)

    // fprintf(stdout, COLOR_WHITE "[TRACE] [%s] [%s:%d:%s] \n\t" fmt COLOR_RESET "\n", \
    //         _log_timestamp(), __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...)
#endif

#endif /* G_LOG_H */

