#ifndef LOGGER_H
#define LOGGER_H

/**
 * @brief Shows a message if the program is being debugged
 * 
 */

#if defined(DEBUG) || !defined(NDEBUG)
extern void libhack_debug(const char *msg, ...);
extern void libhack_warn(const char *msg, ...);
extern void libhack_notice(const char *msg, ...);
extern void libhack_err(const char *msg, ...);
#else
#define libhack_debug(...)
#define libhack_warn(...)
#define libhack_notice(...)
#define libhack_err(...)
#endif

#endif
