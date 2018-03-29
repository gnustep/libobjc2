#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include "selector.h"
#include <stdlib.h>
#include <assert.h>

struct objc_protocol_method_description_list_gcc
{
	/**
	 * Number of method descriptions in this list.
	 */
	int count;
	/**
	 * Methods in this list.  Note: these selectors are NOT resolved.  The name
	 * field points to the name, not to the index of the uniqued version of the
	 * name.  You must not use them for dispatch.
	 */
	struct objc_selector methods[];
};

/**
 * A description of a method in a protocol.
 */
struct objc_protocol_method_description
{
	/**
	 * The selector for this method, includes traditional type encoding.
	 */
	SEL selector;
	/**
	 * The extended type encoding.
	 */
	const char *types;
};

struct objc_protocol_method_description_list
{
	/**
	 * Number of method descriptions in this list.
	 */
	int count;
	/**
	 * Size of `struct objc_method_description`
	 */
	int size;
	/**
	 * Methods in this list.  `count` elements long.
	 */
	struct objc_protocol_method_description methods[];
};

/**
 * Returns a pointer to the method inside the method description list
 * structure.  This structure is designed to allow the compiler to add other
 * fields without breaking the ABI, so although the `methods` field appears to
 * be an array of `objc_protocol_method_description` structures, it may be an
 * array of some future version of these structs, which have fields appended
 * that this version of the runtime does not know about.
 */
static struct objc_protocol_method_description *
protocol_method_at_index(struct objc_protocol_method_description_list *l, int i)
{
	assert(l->size >= sizeof(struct objc_protocol_method_description));
	return (struct objc_protocol_method_description*)(((char*)l->methods) + (i * l->size));
}

struct objc_protocol
{
	/**
	 * Redefinition of the superclass ivars in the C version.
	 */
	id                                   isa;
	char                                *name;
	struct objc_protocol_list           *protocol_list;
	struct objc_protocol_method_description_list *instance_methods;
	struct objc_protocol_method_description_list *class_methods;
	/**
	 * Instance methods that are declared as optional for this protocol.
	 */
	struct objc_protocol_method_description_list *optional_instance_methods;
	/**
	 * Class methods that are declared as optional for this protocol.
	 */
	struct objc_protocol_method_description_list *optional_class_methods;
	/**
	 * Properties that are required by this protocol.
	 */
	struct objc_property_list           *properties;
	/**
	 * Optional properties.
	 */
	struct objc_property_list           *optional_properties;
};


struct objc_protocol_gcc
{
	/** Class pointer. */
	id                                   isa;
	/**
	 * The name of this protocol.  Two protocols are regarded as identical if
	 * they have the same name.
	 */
	char                                *name;
	/**
	 * The list of protocols that this protocol conforms to.
	 */
	struct objc_protocol_list           *protocol_list;
	/**
	 * List of instance methods required by this protocol.
	 */
	struct objc_protocol_method_description_list_gcc *instance_methods;
	/**
	 * List of class methods required by this protocol.
	 */
	struct objc_protocol_method_description_list_gcc *class_methods;
};

struct objc_protocol_gsv1
{
	/**
	 * The first five ivars are shared with `objc_protocol_gcc`.
	 */
	id                                   isa;
	char                                *name;
	struct objc_protocol_list           *protocol_list;
	struct objc_protocol_method_description_list_gcc *instance_methods;
	struct objc_protocol_method_description_list_gcc *class_methods;
	/**
	 * Instance methods that are declared as optional for this protocol.
	 */
	struct objc_protocol_method_description_list_gcc *optional_instance_methods;
	/**
	 * Class methods that are declared as optional for this protocol.
	 */
	struct objc_protocol_method_description_list_gcc *optional_class_methods;
	/**
	 * Properties that are required by this protocol.
	 */
	struct objc_property_list_gsv1      *properties;
	/**
	 * Optional properties.
	 */
	struct objc_property_list_gsv1      *optional_properties;
};

// Note: If you introduce a new protocol type that is larger than the current
// one then it's fine to auto-upgrade anything using the v2 ABI, because
// protocol structures there are referenced only via the indirection layer or
// via other runtime-managed structures.
//
// Auto-upgrading GNUstep v1 ABI protocols relies on their being the same size
// as v2, so the upgrade can happen in place.  If this isn't possible, then we
// will need to add a new protocol class for v1 ABI struct and make sure that
// anything accessing the missing fields checks for this class before doing so.
_Static_assert(sizeof(struct objc_protocol_gsv1) == sizeof(struct objc_protocol),
	"The V1 ABI protocol strcuture has a different size to the current AIB.  "
	"The auto-upgrader will not be able to do in-place replacement.");

#ifdef __OBJC__
@interface Object { id isa; } @end
/**
 * Definition of the Protocol type.  Protocols are objects, but are rarely used
 * as such.
 */
@interface Protocol : Object
@end

@interface ProtocolGCC : Protocol
@end

#endif

/**
 * List of protocols.  Attached to a class or a category by the compiler and to
 * a class by the runtime.
 */
struct objc_protocol_list
{
	/**
	 * Additional protocol lists.  Loading a category that declares protocols
	 * will cause a new list to be prepended using this pointer to the protocol
	 * list for the class.  Unlike methods, protocols can not be overridden,
	 * although it is possible for a protocol to appear twice.
	 */
	struct objc_protocol_list *next;
	/**
	 * The number of protocols in this list.
	 */
	size_t                     count;
	/**
	 * An array of protocols.  Contains `count` elements.
	 *
	 * On load, this contains direct references to other protocols and should
	 * be updated to point to the canonical (possibly upgraded) version.
	 */
	struct objc_protocol      *list[];
};

#endif // PROTOCOL_H_INCLUDED
