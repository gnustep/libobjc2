#include "objc/objc-visibility.h"

#if defined _WIN32 || defined __CYGWIN__
#	define PRIVATE
#else
#	define PRIVATE  __attribute__ ((visibility("hidden")))
#endif
#ifdef NO_LEGACY
#	define LEGACY PRIVATE
#else
#	define LEGACY OBJC_PUBLIC
#endif

#if defined(DEBUG) || (!defined(__clang__))
#	include <assert.h>
#	define UNREACHABLE(x) assert(0 && x)
#	define ASSERT(x) assert(x)
#else
#	define UNREACHABLE(x) __builtin_unreachable()
#	define ASSERT(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#endif

#define LIKELY(x) __builtin_expect(x, 1)
#define UNLIKELY(x) __builtin_expect(x, 0)
