/**
 * This file includes all of the hooks that can be used to alter the behaviour
 * of the runtime.  
 */


#ifndef OBJC_HOOK
#define OBJC_HOOK extern
#endif
struct objc_category;
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
/**
 * Class load callback.  
 */
OBJC_HOOK void (*_objc_load_callback)(Class class, struct objc_category *category);
/**
 * The hook used for fast proxy lookups.  This takes an object and a selector
 * and returns the instance that the message should be forwarded to.
 */
OBJC_HOOK id (*objc_proxy_lookup)(id receiver, SEL op);
/**
 * New runtime forwarding hook.  This might be removed in future - it's
 * actually no more expressive than the forward2 hook and forces Foundation to
 * do some stuff that the runtime is better suited to.
 */
OBJC_HOOK struct objc_slot *(*__objc_msg_forward3)(id, SEL);
/**
 * Forwarding hook.  Takes an object and a selector and returns a method that
 * handles the forwarding.
 */
OBJC_HOOK IMP (*__objc_msg_forward2)(id, SEL);
/**
 * Hook defined for handling unhandled exceptions.  If the unwind library
 * reaches the end of the stack without finding a handler then this hook is
 * called.
 */
OBJC_HOOK void (*_objc_unexpected_exception)(id exception);
