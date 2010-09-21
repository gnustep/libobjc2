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

typedef struct objc_ivar* Ivar;

// Don't redefine these types if the old GNU header was included first.
#ifndef __objc_INCLUDE_GNU
// Define the macro so that including the old GNU header does nothing.
#	define __objc_INCLUDE_GNU
#	define __objc_api_INCLUDE_GNU


#if !defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
typedef const struct objc_selector *SEL;
#else
typedef struct objc_selector *SEL;
#endif

typedef struct objc_class *Class;

typedef struct objc_object
{
	Class isa;
} *id;

struct objc_super
{
	id receiver;
#	if !defined(__cplusplus)  &&  !__OBJC2__
	Class class;
#	else
	Class super_class;
#	endif
};

typedef id (*IMP)(id, SEL, ...);
typedef struct objc_method *Method;

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


struct objc_property;
typedef struct objc_property* objc_property_t;
#ifdef __OBJC__
@class Protocol;
#else
typedef struct objc_protocol Protocol;
#endif

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

BOOL class_addIvar(Class cls,
                   const char *name,
                   size_t size,
                   uint8_t alignment,
                   const char *types);

BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types);

BOOL class_addProtocol(Class cls, Protocol *protocol);

BOOL class_conformsToProtocol(Class cls, Protocol *protocol);

Ivar * class_copyIvarList(Class cls, unsigned int *outCount);

Method * class_copyMethodList(Class cls, unsigned int *outCount);

objc_property_t* class_copyPropertyList(Class cls, unsigned int *outCount);

Protocol ** class_copyProtocolList(Class cls, unsigned int *outCount);

id class_createInstance(Class cls, size_t extraBytes);

Method class_getClassMethod(Class aClass, SEL aSelector);

Ivar class_getClassVariable(Class cls, const char* name);

Method class_getInstanceMethod(Class aClass, SEL aSelector);

size_t class_getInstanceSize(Class cls);

/** Look up the named instance variable in the class (and its superclasses)
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

OBJC_NONPORTABLE
Class objc_allocateMetaClass(Class superclass, size_t extraBytes);

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

// Global self so that self is a valid symbol everywhere.  Will be replaced by
// a real self in an inner scope if there is one.
static const id self = nil;
// This uses a GCC extension, but the is no good way of doing it otherwise.
#define objc_msgSend(theReceiver, theSelector, ...) \
({\
	id __receiver = theReceiver;\
	SEL op = theSelector;\
	objc_msg_lookup_sender(&__receiver, op, self)(__receiver, op, ## __VA_ARGS__);\
})

#define objc_msgSendSuper(super, op, ...) objc_msg_lookup_super(super, op)((super)->receiver, op, ## __VA_ARGS__)

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
