/**
 * capabilities.h - This file defines the list of capabilities.  Runtime
 * capabilities can be checked.  You may use #ifdef to test at compile time
 * whether the runtime on the current platform understands the capability.
 * This does not mean that the runtime implements the capability, however.  
 *
 * A copy of this file exists for compatibility in GNUstep's Objective-C
 * framework.  When using this framework in conjunction with the GNU
 * Objective-C runtime, most of the features will not be supported at run time,
 * even if the corresponding macros are available at compile time.
 * Additionally, several are compile-time options in the GNUstep runtime, so
 * although they are present in the header and understood by the runtime, they
 * may not be supported by the installed runtime.  
 */
#include "Availability.h"

#ifndef __GNUSTEP_CAPABILITIES_H__
#	define __GNUSTEP_CAPABILITIES_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The runtime supports zero-cost exceptions.  
 */
#define OBJC_CAP_EXCEPTIONS              0
/**
 * The runtime supports the @synchronize directive.
 */
#define OBJC_CAP_SYNCRONIZE              1
/**
 * The runtime supports property accessors.
 */
#define OBJC_CAP_PROPERTIES              2
/**
 * The runtime supports introspection on declared properties.
 */
#define OBJC_CAP_PROPERTY_INTROSPECTION  3
/**
 * The runtime supports optional methods and declared properties in protocols.
 */
#define OBJC_CAP_OPTIONAL_PROTOCOLS      4
/**
 * The runtime supports non-fragile instance variables.  
 */
#define OBJC_CAP_NONFRAGILE_IVARS        5
/**
 * The runtime supports making method lookup dependent on the types, as well as
 * the name, of the selector.  
 */
#define OBJC_CAP_TYPE_DEPENDENT_DISPATCH 6
/**
 * The runtime was compiled in the low-memory profile.  This trades some speed
 * for reduced memory consumption.
 */
#define OBJC_CAP_LOW_MEMORY              7
/**
 * The runtime supports developer mode.  When in user mode (the default),
 * loading two classes with the same name will cause the program to abort.  In
 * developer mode, the new class will replace the old one.  If the ivar layouts
 * are the same, the new class will be treated as a category.  If they are
 * different, then it will replace the old one in the class table, meaning that
 * message sends to the class will go to the new version, but existing
 * instances will not acquire the new methods.
 */
#define OBJC_DEVELOPER_MODE              8
/**
 * This runtime supports the unified exception model.  This means that
 * Objective-C objects can be caught by either Objective-C or C++ exception
 * handlers (the latter only in Objective-C++ code), irrespective of whether
 * they are thrown from C++ throw of Objective-C @throw statements.
 */
#define OBJC_UNIFIED_EXCEPTION_MODEL     9

/**
 * Macro used to require the existence of a specific capability.  This creates
 * a function that is called by the loader and tests that the runtime supports
 * the required capability, aborting if it does not.
 */
#define OBJC_REQUIRE_CAPABILITY(x) \
	__attribute__((constructor)) static void objc_test ## x(void)\
	{\
		if (!objc_test_capability(x))\
		{\
			fprintf(stderr, "Runtime does not support required feature: " #x "\n");\
			exit(1);\
		}\
	}

/**
 * Run time feature test.  This function returns 1 if the runtime supports the
 * specified feature or 0 if it does not.
 */
int objc_test_capability(int x) OBJC_NONPORTABLE;
#ifdef __cplusplus
}
#endif

#endif //__GNUSTEP_CAPABILITIES_H__
