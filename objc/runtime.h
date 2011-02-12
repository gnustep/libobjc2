#ifndef __LIBOBJC_RUNTIME_H_INCLUDED__
#define __LIBOBJC_RUNTIME_H_INCLUDED__

#ifndef __GNUSTEP_RUNTIME__
#	define __GNUSTEP_RUNTIME__
#endif



#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "Availability.h"

// Undo GNUstep substitutions
#ifdef class_setVersion 
#	undef class_setVersion
#endif
#ifdef class_getClassMethod
#	undef class_getClassMethod
#endif
#ifdef objc_getClass
#	undef objc_getClass
#endif
#ifdef objc_lookUpClass
#	undef objc_lookUpClass
#endif

/**
 * Opaque type for Objective-C instance variable metadata.
 */
typedef struct objc_ivar* Ivar;

// Don't redefine these types if the old GCC header was included first.
#ifndef __objc_INCLUDE_GNU
// Define the macro so that including the old GCC header does nothing.
#	define __objc_INCLUDE_GNU
#	define __objc_api_INCLUDE_GNU


/**
 * Opaque type used for selectors.
 */
#if !defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
typedef const struct objc_selector *SEL;
#else
typedef struct objc_selector *SEL;
#endif

/**
 * Opaque type for Objective-C classes.
 */
typedef struct objc_class *Class;

/**
 * Type for Objective-C objects.  
 */
typedef struct objc_object
{
	/**
	 * Pointer to this object's class.  Accessing this directly is STRONGLY
	 * discouraged.  You are recommended to use object_getClass() instead.
	 */
	Class isa;
} *id;

/**
 * Structure used for calling superclass methods.
 */
struct objc_super
{
	/** The receiver of the message. */
	id receiver;
	/** The class containing the method to call. */
#	if !defined(__cplusplus)  &&  !__OBJC2__
	Class class;
#	else
	Class super_class;
#	endif
};

/**
 * Instance Method Pointer type.  Note: Since the calling convention for
 * variadic functions sometimes differs from the calling convention for
 * non-variadic functions, you must cast an IMP to the correct type before
 * calling.
 */
typedef id (*IMP)(id, SEL, ...);
/**
 * Opaque type for Objective-C method metadata.
 */
typedef struct objc_method *Method;

/**
 * Objective-C boolean type.  
 */
#	ifdef STRICT_APPLE_COMPATIBILITY
typedef signed char BOOL;
#	else
#		ifdef __vxwords
typedef  int BOOL
#		else
typedef unsigned char BOOL;
#		endif
#	endif

#else
// Method in the GNU runtime is a struct, Method_t is the pointer
#	define Method Method_t
#endif // __objc_INCLUDE_GNU


/**
 * Opaque type for Objective-C property metadata.
 */
typedef struct objc_property* objc_property_t;
/**
 * Opaque type for Objective-C protocols.  Note that, although protocols are
 * objects, sending messages to them is deprecated in Objective-C 2 and may not
 * work in the future.
 */
#ifdef __OBJC__
@class Protocol;
#else
typedef struct objc_protocol Protocol;
#endif

/**
 * Objective-C method description.
 */
struct objc_method_description
{
	/**
	 * The name of this method.
	 */
	SEL   name;
	/**
	 * The types of this method.
	 */
	const char *types;
};


#ifndef YES
#	define YES ((BOOL)1)
#endif
#ifndef NO 
#	define NO ((BOOL)0)
#endif

#ifdef __GNUC
#	define _OBJC_NULL_PTR __null
#elif defined(__cplusplus)
#	define _OBJC_NULL_PTR 0
#else
#	define _OBJC_NULL_PTR ((void*)0)
#endif

#ifndef nil
#	define nil ((id)_OBJC_NULL_PTR)
#endif

#ifndef Nil
#	define Nil ((Class)_OBJC_NULL_PTR)
#endif

#include "slot.h"

/**
 * Adds an instance variable to the named class.  The class must not have been
 * registered by the runtime.  The alignment must be the base-2 logarithm of
 * the alignment requirement and the types should be an Objective-C type encoding.
 */
BOOL class_addIvar(Class cls,
                   const char *name,
                   size_t size,
                   uint8_t alignment,
                   const char *types);

/**
 * Adds a method to the class.  
 */
BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types);

/**
 * Adds a protocol to the class.
 */
BOOL class_addProtocol(Class cls, Protocol *protocol);

/**
 * Tests for protocol conformance.  Note: Currently, protocols with the same
 * name are regarded as equivalent, even if they have different methods.  This
 * behaviour will change in a future version.
 */
BOOL class_conformsToProtocol(Class cls, Protocol *protocol);

/**
 * Copies the instance variable list for this class.  The integer pointed to by
 * the outCount argument is set to the number of instance variables returned.
 * The caller is responsible for freeing the returned buffer.
 */
Ivar* class_copyIvarList(Class cls, unsigned int *outCount);

Method * class_copyMethodList(Class cls, unsigned int *outCount);

objc_property_t* class_copyPropertyList(Class cls, unsigned int *outCount);

Protocol ** class_copyProtocolList(Class cls, unsigned int *outCount);

id class_createInstance(Class cls, size_t extraBytes);

Method class_getClassMethod(Class aClass, SEL aSelector);

Ivar class_getClassVariable(Class cls, const char* name);

Method class_getInstanceMethod(Class aClass, SEL aSelector);

size_t class_getInstanceSize(Class cls);

/**
 * Look up the named instance variable in the class (and its superclasses)
 * returning a pointer to the instance variable definition or a null
 * pointer if no instance variable of that name was found.
 */
Ivar class_getInstanceVariable(Class cls, const char* name);

const char *class_getIvarLayout(Class cls);

IMP class_getMethodImplementation(Class cls, SEL name);

IMP class_getMethodImplementation_stret(Class cls, SEL name);

const char * class_getName(Class cls);

objc_property_t class_getProperty(Class cls, const char *name);

Class class_getSuperclass(Class cls);

int class_getVersion(Class theClass);

OBJC_GNUSTEP_RUNTIME_UNSUPPORTED("Weak instance variables")
const char *class_getWeakIvarLayout(Class cls);

BOOL class_isMetaClass(Class cls);

IMP class_replaceMethod(Class cls, SEL name, IMP imp, const char *types);

BOOL class_respondsToSelector(Class cls, SEL sel);

void class_setIvarLayout(Class cls, const char *layout);

__attribute__((deprecated))
Class class_setSuperclass(Class cls, Class newSuper);

void class_setVersion(Class theClass, int version);

OBJC_GNUSTEP_RUNTIME_UNSUPPORTED("Weak instance variables")
void class_setWeakIvarLayout(Class cls, const char *layout);

const char * ivar_getName(Ivar ivar);

ptrdiff_t ivar_getOffset(Ivar ivar);

const char * ivar_getTypeEncoding(Ivar ivar);

char * method_copyArgumentType(Method method, unsigned int index);

char * method_copyReturnType(Method method);

void method_exchangeImplementations(Method m1, Method m2);

void method_getArgumentType(Method method, unsigned int index, char *dst, size_t dst_len);

IMP method_getImplementation(Method method);

SEL method_getName(Method method);

unsigned method_getNumberOfArguments(Method method);

void method_getReturnType(Method method, char *dst, size_t dst_len);

const char * method_getTypeEncoding(Method method);

IMP method_setImplementation(Method method, IMP imp);

Class objc_allocateClassPair(Class superclass, const char *name, size_t extraBytes);

void objc_disposeClassPair(Class cls);

id objc_getClass(const char *name);

int objc_getClassList(Class *buffer, int bufferLen);

id objc_getMetaClass(const char *name);

id objc_getRequiredClass(const char *name);

id objc_lookUpClass(const char *name);

Class objc_allocateClassPair(Class superclass, const char *name, size_t extraBytes);

Protocol *objc_getProtocol(const char *name);

void objc_registerClassPair(Class cls);

void *object_getIndexedIvars(id obj);

// FIXME: The GNU runtime has a version of this which omits the size parameter
//id object_copy(id obj, size_t size);

id object_dispose(id obj);

Class object_getClass(id obj);
Class object_setClass(id obj, Class cls);

const char *object_getClassName(id obj);

IMP objc_msg_lookup(id, SEL) OBJC_NONPORTABLE;
IMP objc_msg_lookup_super(struct objc_super*, SEL) OBJC_NONPORTABLE;

const char *property_getName(objc_property_t property);

BOOL protocol_conformsToProtocol(Protocol *p, Protocol *other);

struct objc_method_description *protocol_copyMethodDescriptionList(Protocol *p,
	BOOL isRequiredMethod, BOOL isInstanceMethod, unsigned int *count);

objc_property_t *protocol_copyPropertyList(Protocol *p, unsigned int *count);

Protocol **protocol_copyProtocolList(Protocol *p, unsigned int *count);

struct objc_method_description protocol_getMethodDescription(Protocol *p,
	SEL aSel, BOOL isRequiredMethod, BOOL isInstanceMethod);

const char *protocol_getName(Protocol *p);

/**
 * Note: The Apple documentation for this method contains some nonsense for
 * isInstanceProperty.  As there is no language syntax for defining properties
 * on classes, we return NULL if this is not YES.
 */
objc_property_t protocol_getProperty(Protocol *p, const char *name,
	BOOL isRequiredProperty, BOOL isInstanceProperty);

BOOL protocol_isEqual(Protocol *p, Protocol *other);

const char *sel_getName(SEL sel);

SEL sel_getUid(const char *selName);

/**
 * Returns whether two selectors are equal.  For the purpose of comparison,
 * selectors with the same name and type are regarded as equal.  Selectors with
 * the same name and different types are regarded as different.  If one
 * selector is typed and the other is untyped, but the names are the same, then
 * they are regarded as equal.  This means that sel_isEqual(a, b) and
 * sel_isEqual(a, c) does not imply sel_isEqual(b, c) - if a is untyped but
 * both b and c are typed selectors with different types, then then the first
 * two will return YES, but the third case will return NO.
 */
BOOL sel_isEqual(SEL sel1, SEL sel2);

SEL sel_registerName(const char *selName);

/**
 * Register a typed selector.
 */
SEL sel_registerTypedName_np(const char *selName, const char *types) OBJC_NONPORTABLE;

/**
 * Returns the type encoding associated with a selector, or the empty string is
 * there is no such type.
 */
const char *sel_getType_np(SEL aSel) OBJC_NONPORTABLE;

/**
 * Enumerates all of the type encodings associated with a given selector name
 * (up to a specified limit).  This function returns the number of types that
 * exist for a specific selector, but only copies up to count of them into the
 * array passed as the types argument.  This allows you to call the function
 * once with a relatively small on-stack buffer and then only call it again
 * with a heap-allocated buffer if there is not enough space.
 */
unsigned sel_copyTypes_np(const char *selName, const char **types, unsigned count) OBJC_NONPORTABLE;

/**
 * New ABI lookup function.  Receiver may be modified during lookup or proxy
 * forwarding and the sender may affect how lookup occurs.
 */
extern struct objc_slot *objc_msg_lookup_sender(id *receiver, SEL selector, id sender)
	OBJC_NONPORTABLE;

/**
 * Legacy GNU runtime compatibility.
 *
 * All of the functions in this section are deprecated and should not be used
 * in new code.
 */
__attribute__((deprecated))
void *objc_malloc(size_t size);

__attribute__((deprecated))
void *objc_atomic_malloc(size_t size);

__attribute__((deprecated))
void *objc_valloc(size_t size);

__attribute__((deprecated))
void *objc_realloc(void *mem, size_t size);

__attribute__((deprecated))
void * objc_calloc(size_t nelem, size_t size);

__attribute__((deprecated))
void objc_free(void *mem);

__attribute__((deprecated))
id objc_get_class(const char *name);

__attribute__((deprecated))
id objc_lookup_class(const char *name);

__attribute__((deprecated))
id objc_get_meta_class(const char *name);

#if !defined(__OBJC_RUNTIME_INTERNAL__)
__attribute__((deprecated))
#endif
Class objc_next_class(void **enum_state);

__attribute__((deprecated))
Class class_pose_as(Class impostor, Class super_class);

__attribute__((deprecated))
SEL sel_get_typed_uid (const char *name, const char *types);

__attribute__((deprecated))
SEL sel_get_any_typed_uid (const char *name);

__attribute__((deprecated))
SEL sel_get_any_uid (const char *name);

__attribute__((deprecated))
SEL sel_get_uid(const char *name);

__attribute__((deprecated))
const char *sel_get_name(SEL selector);

#if !defined(__OBJC_RUNTIME_INTERNAL__)
__attribute__((deprecated))
#endif
BOOL sel_is_mapped(SEL selector);

__attribute__((deprecated))
const char *sel_get_type(SEL selector);

__attribute__((deprecated))
SEL sel_register_name(const char *name);

__attribute__((deprecated))
SEL sel_register_typed_name(const char *name, const char *type);

__attribute__((deprecated))
BOOL sel_eq(SEL s1, SEL s2);



#define _C_ID       '@'
#define _C_CLASS    '#'
#define _C_SEL      ':'
#define _C_BOOL     'B'

#define _C_CHR      'c'
#define _C_UCHR     'C'
#define _C_SHT      's'
#define _C_USHT     'S'
#define _C_INT      'i'
#define _C_UINT     'I'
#define _C_LNG      'l'
#define _C_ULNG     'L'
#define _C_LNG_LNG  'q'
#define _C_ULNG_LNG 'Q'

#define _C_FLT      'f'
#define _C_DBL      'd'

#define _C_BFLD     'b'
#define _C_VOID     'v'
#define _C_UNDEF    '?'
#define _C_PTR      '^'

#define _C_CHARPTR  '*'
#define _C_ATOM     '%'

#define _C_ARY_B    '['
#define _C_ARY_E    ']'
#define _C_UNION_B  '('
#define _C_UNION_E  ')'
#define _C_STRUCT_B '{'
#define _C_STRUCT_E '}'
#define _C_VECTOR   '!'

#define _C_COMPLEX  'j'
#define _C_CONST    'r'
#define _C_IN       'n'
#define _C_INOUT    'N'
#define _C_OUT      'o'
#define _C_BYCOPY   'O'
#define _C_ONEWAY   'V'


#endif // __LIBOBJC_RUNTIME_H_INCLUDED__
