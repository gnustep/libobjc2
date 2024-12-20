/**
 * Shims around non-standardized platform-specific APIs
 */

#ifndef SHIMS_H_INCLUDED
#define SHIMS_H_INCLUDED

#if defined(_WIN32)

/**
 * getpagesize() returns the current page size
 */
int getpagesize(void);

#else // Assume that the platform implements the X/Open System Interface

#include <unistd.h>

#endif // defined(_WIN32)

#endif // SHIMS_H_INCLUDED
