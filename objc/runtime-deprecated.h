#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#if !defined(__GNUSTEP_LIBOBJC_RUNTIME_DEPRECATED_INCLUDED__) && !defined(GNUSTEP_LIBOBJC_NO_LEGACY)
#	define __GNUSTEP_LIBOBJC_RUNTIME_DEPRECATED_INCLUDED__

/**
 * Legacy GNU runtime compatibility.
 *
 * All of the functions in this section are deprecated and should not be used
 * in new code.
 */

OBJC_PUBLIC
__attribute__((deprecated))
void *objc_malloc(size_t size);

OBJC_PUBLIC
__attribute__((deprecated))
void *objc_atomic_malloc(size_t size);

OBJC_PUBLIC
__attribute__((deprecated))
void *objc_valloc(size_t size);

OBJC_PUBLIC
__attribute__((deprecated))
void *objc_realloc(void *mem, size_t size);

OBJC_PUBLIC
__attribute__((deprecated))
void * objc_calloc(size_t nelem, size_t size);

OBJC_PUBLIC
__attribute__((deprecated))
void objc_free(void *mem);

OBJC_PUBLIC
__attribute__((deprecated))
id objc_get_class(const char *name);

OBJC_PUBLIC
__attribute__((deprecated))
id objc_lookup_class(const char *name);

OBJC_PUBLIC
__attribute__((deprecated))
id objc_get_meta_class(const char *name);

OBJC_PUBLIC
#if !defined(__OBJC_RUNTIME_INTERNAL__)
__attribute__((deprecated))
#endif
Class objc_next_class(void **enum_state);

OBJC_PUBLIC
__attribute__((deprecated))
Class class_pose_as(Class impostor, Class super_class);

OBJC_PUBLIC
__attribute__((deprecated))
SEL sel_get_typed_uid (const char *name, const char *types);

OBJC_PUBLIC
__attribute__((deprecated))
SEL sel_get_any_typed_uid (const char *name);

OBJC_PUBLIC
__attribute__((deprecated))
SEL sel_get_any_uid (const char *name);

OBJC_PUBLIC
__attribute__((deprecated))
SEL sel_get_uid(const char *name);

OBJC_PUBLIC
__attribute__((deprecated))
const char *sel_get_name(SEL selector);

OBJC_PUBLIC
#if !defined(__OBJC_RUNTIME_INTERNAL__)
__attribute__((deprecated))
#endif
BOOL sel_is_mapped(SEL selector);

OBJC_PUBLIC
__attribute__((deprecated))
const char *sel_get_type(SEL selector);

OBJC_PUBLIC
__attribute__((deprecated))
SEL sel_register_name(const char *name);

OBJC_PUBLIC
__attribute__((deprecated))
SEL sel_register_typed_name(const char *name, const char *type);

OBJC_PUBLIC
__attribute__((deprecated))
BOOL sel_eq(SEL s1, SEL s2);

#endif
