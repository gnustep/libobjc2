#ifdef __cplusplus
extern "C" {
#endif
/**
 * Allocates a C++ exception.  This function is part of the Itanium C++ ABI and
 * is provided externally.
 */
void *__cxa_allocate_exception(size_t thrown_size);
/**
 * Initialises an exception object returned by __cxa_allocate_exception() for
 * storing an Objective-C object.  The return value is the location of the
 * _Unwind_Exception structure within this structure, and should be passed to
 * the C++ personality function.
 */
struct _Unwind_Exception *objc_init_cxx_exception(void *thrown_exception);
/**
 * The GNU C++ exception personality function, provided by libsupc++ (GNU) or
 * libcxxrt (PathScale).
 */
_Unwind_Reason_Code  __gxx_personality_v0(int version,
                                          _Unwind_Action actions,
                                          uint64_t exceptionClass,
                                          struct _Unwind_Exception *exceptionObject,
                                          struct _Unwind_Context *context);
/**
 * Frees an exception object allocated by __cxa_allocate_exception().  Part of
 * the Itanium C++ ABI.
 */
void __cxa_free_exception(void *thrown_exception);
/**
 * Tests whether a C++ exception contains an Objective-C object, and returns if
 * if it does.  Returns -1 if it doesn't.  -1 is used instead of 0, because
 * throwing nil is allowed, but throwing non-nil, invalid objects is not.
 */
void *objc_object_for_cxx_exception(void *thrown_exception);

/**
 * Prints the type info associated with an exception.  Used only when
 * debugging, not compiled in the normal build.
 */
void print_type_info(void *thrown_exception);


#ifdef __cplusplus
}
#endif
