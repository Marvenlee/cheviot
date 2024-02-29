
#ifndef BOARDS_RASPBERRY_PI_1_DEBUG_H
#define BOARDS_RASPBERRY_PI_1_DEBUG_H

#include <machine/cheviot_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

//#define NDEBUG

#if !defined(NDEBUG)

/* @brief   Debug level constants
 *
 * Define one of these log level constants at the top of a source file and include
 * this sys/debug.h header file.  Ensure that NDEBUG is not defined.
 *
 * - LOG_LEVEL_ERROR
 * - LOG_LEVEL_WARN
 * - LOG_LEVEL_INFO
 * - LOG_LEVEL_DEBUG
 * - LOG_LEVEL_TRACE
 */

// Set a default log level if none are defined
#if (!defined(LOG_LEVEL_ERROR) && !defined(LOG_LEVEL_WARN) \
      && !defined(LOG_LEVEL_INFO) && !defined(LOG_LEVEL_DEBUG) \
      && !defined(LOG_LEVEL_TRACE))
#define LOG_LEVEL_INFO
#endif

#if defined(LOG_LEVEL_TRACE)
#define LOG_LEVEL_DEBUG
#endif

#if defined(LOG_LEVEL_DEBUG)
#define LOG_LEVEL_INFO
#endif

#if defined(LOG_LEVEL_INFO)
#define LOG_LEVEL_WARN
#endif

#if defined(LOG_LEVEL_WARN)
#define LOG_LEVEL_ERROR
#endif


/*
 * Prototypes
 */
void boot_debug_init(void);
void boot_debug_printf(const char *format, ...);
void boot_panic(char *string);
void boot_debug_write_string(char *string);
;

/*
 *
 */
#if defined(LOG_LEVEL_ERROR)
#define boot_log_error(fmt, args...)   boot_debug_printf(fmt, ##args)
#endif

#if defined(LOG_LEVEL_WARN)
#define boot_log_warn(fmt, args...)    boot_debug_printf(fmt, ##args)
#endif

#if defined(LOG_LEVEL_INFO)
#define boot_log_info(fmt, args...)    boot_debug_printf(fmt, ##args)
#endif

#if defined(LOG_LEVEL_DEBUG)
#define boot_log_debug(fmt, args...)   boot_debug_printf(fmt, ##args)
#endif

#if defined(LOG_LEVEL_TRACE)
#define boot_log_trace(fmt, args...)   boot_debug_printf(fmt, ##args)
#endif

#else

#define boot_log_error(fmt, args...)
#define boot_log_warn(fmt, args...)
#define boot_log_info(fmt, args...)
#define boot_log_debug(fmt, args...)
#define boot_log_trace(fmt, args...)

#endif

#endif
