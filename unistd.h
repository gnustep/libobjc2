/* See http://llvm.org/bugs/show_bug.cgi?id=4746 */
#ifdef __block
#	undef __block
#	include_next "unistd.h"
#	define __block __attribute__((__blocks__(byref)))
#else
#	if __has_include_next("unitstd.h")
#		include_next "unistd.h"
#	endif
#endif
