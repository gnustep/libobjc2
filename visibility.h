#if defined _WIN32 || defined __CYGWIN__
#	define PUBLIC __attribute__((dllexport))
#	define PRIVATE
#else
#	define PUBLIC __attribute__ ((visibility("default")))
#	define PRIVATE  __attribute__ ((visibility("hidden")))
#endif

