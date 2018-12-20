/**
 * @file include/assert.h
 * @brief Provides assert interface used for internal error detection.
 */

/* it's actually OK to include this file multiple times; we may
 * change the value of NDEBUG in between, too
 */
#include <compiler.h>

#undef assert

#if defined(__cplusplus)
extern "C" {
#endif

_NORETURN void abort(void);
extern void __eprintf( const char *expr, unsigned long line, const char *filename);

/**
 * @brief Abort the program if the assertion is false
 * @details
 * If macro NDEBUG is defined at the moment <assert.h> was last included the assert()
 * macro genenrates no code and does nothing.
 * @details
 * Else assert() prints an error message to stderr and kills the program by calling
 * abort() if cond is false.
 *
 * @param cond The condition tested
 */
#if defined(NDEBUG)
#define assert(cond) (void)(0)
#else
#define assert(cond) \
  ((cond) ? 0 : (__eprintf(#cond,(long)(__LINE__), __FILE__), abort(), 0))
#endif

#if defined(__cplusplus)
}
#endif
