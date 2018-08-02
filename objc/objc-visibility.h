#if defined _WIN32 || defined __CYGWIN__
#	ifdef __OBJC_RUNTIME_INTERNAL__
#		define PUBLIC __attribute__((dllexport))
#	else
#		define PUBLIC __attribute__((dllimport))
#	endif
#else
#	define PUBLIC
#endif
