/**
 * This file includes all of the hooks that can be used to alter the behaviour
 * of the runtime.  
 */


#ifndef OBJC_HOOK
#define OBJC_HOOK extern
#endif

/**
 * Class lookup hook.  Set this to provide a mechanism for resolving classes
 * that have not been registered with the runtime.  This can be used for lazy
 * library loading, for example.  The hook takes a class name as an argument
 * and returns the class.  A JIT compiler could use this to allow classes to be
 * compiled the first time that they are looked up.  If the class is already
 * registered with the runtime, this will not be called, so it can not be used
 * for lazy loading of categories.
 */
OBJC_HOOK Class (*_objc_lookup_class)(const char *name);
