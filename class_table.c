#include "magic_objects.h"
#include "objc/runtime.h"
#include "objc/hooks.h"
#include "class.h"
#include "method_list.h"
#include "selector.h"
#include "lock.h"
#include <stdlib.h>
#include <assert.h>

void __objc_register_selectors_from_class(Class class);
void *__objc_uninstalled_dtable;
void __objc_init_protocols(struct objc_protocol_list *protos);
void __objc_compute_ivar_offsets(Class class);

////////////////////////////////////////////////////////////////////////////////
// +load method hash table
////////////////////////////////////////////////////////////////////////////////
static int imp_compare(IMP i1, IMP i2)
{
	return i1 == i2;
}
static int imp_hash(const IMP imp)
{
	return ((int)imp) >> 4;
}
#define MAP_TABLE_NAME load_messages
#define MAP_TABLE_COMPARE_FUNCTION imp_compare
#define MAP_TABLE_HASH_KEY imp_hash
#define MAP_TABLE_HASH_VALUE imp_hash
#include "hash_table.h"

static load_messages_table *load_table;

SEL loadSel;

void objc_init_load_messages_table(void)
{
	load_table  = load_messages_create(4096);
	loadSel = sel_registerName("load");
}

void objc_send_load_message(Class class)
{
	for (struct objc_method_list *l=class->methods ; NULL!=l ; l=l->next)
	{
		for (int i=0 ; i<l->count ; i++)
		{
			Method m = &l->methods[i];
			if (m->selector->name == loadSel->name)
			{
				if (load_messages_table_get(load_table, m->imp) == 0)
				{
					m->imp((id)class, loadSel);
					load_messages_insert(load_table, m->imp);
				}
			}
		}
	}
}

// Get the functions for string hashing
#include "string_hash.h"

static int class_compare(const char *name, const Class class)
{
	return string_compare(name, class->name);
}
static int class_hash(const Class class)
{
	return string_hash(class->name);
}
#define MAP_TABLE_NAME class_table_internal
#define MAP_TABLE_COMPARE_FUNCTION class_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE class_hash
// This defines the maximum number of classes that the runtime supports.
/*
#define MAP_TABLE_STATIC_SIZE 2048
#define MAP_TABLE_STATIC_NAME class_table
*/
#include "hash_table.h"

static class_table_internal_table *class_table;


#define unresolved_class_next subclass_list
#define unresolved_class_prev sibling_class
/**
 * Linked list using the subclass_list pointer in unresolved classes.
 */
static Class unresolved_class_list;

////////////////////////////////////////////////////////////////////////////////
// Class table manipulation
////////////////////////////////////////////////////////////////////////////////

void class_table_insert(Class class)
{
	if (!objc_test_class_flag(class, objc_class_flag_resolved))
	{
		if (Nil != unresolved_class_list)
		{
			unresolved_class_list->unresolved_class_prev = class;
		}
		class->unresolved_class_next = unresolved_class_list;
		unresolved_class_list = class;
	}
	class_table_internal_insert(class_table, class);
}

Class class_table_get_safe(const char *class_name)
{
	return class_table_internal_table_get(class_table, class_name);
}

Class class_table_next(void **e)
{
	return class_table_internal_next(class_table, 
			(struct class_table_internal_table_enumerator**)e);
}

void __objc_init_class_tables(void)
{
	class_table = class_table_internal_create(4096);
	objc_init_load_messages_table();
}

////////////////////////////////////////////////////////////////////////////////
// Loader functions
////////////////////////////////////////////////////////////////////////////////

BOOL  objc_resolve_class(Class cls)
{
	// Skip this if the class is already resolved.
	if (objc_test_class_flag(cls, objc_class_flag_resolved)) { return YES; }

	// We can only resolve the class if its superclass is resolved.
	if (cls->super_class)
	{
		Class super = (Class)objc_getClass((char*)cls->super_class);
		if (Nil == super) { return NO; }

		if (!objc_test_class_flag(cls, objc_class_flag_resolved))
		{
			if (!objc_resolve_class(super))
			{
				return NO;
			}
		}
	}


	// Give up if we can't resolve the root class yet...
	static Class root_class = Nil;
	if (Nil == root_class)
	{
		root_class = (Class)objc_getClass(ROOT_OBJECT_CLASS_NAME);
		if (Nil == root_class) { return NO; }

		if (!objc_test_class_flag(root_class, objc_class_flag_resolved))
		{
			objc_resolve_class(root_class);
		}
		assert(root_class);
	}


	// Remove the class from the unresolved class list
	if (Nil == cls->unresolved_class_prev)
	{
		unresolved_class_list = cls->unresolved_class_next;
	}
	else
	{
		cls->unresolved_class_prev->unresolved_class_next =
			cls->unresolved_class_next;
	}
	if (Nil != cls->unresolved_class_next)
	{
		cls->unresolved_class_next->unresolved_class_prev = 
			cls->unresolved_class_prev;
	}
	cls->unresolved_class_prev = Nil;
	cls->unresolved_class_next = Nil;

	// Resolve the superclass pointer

	// If this class has no superclass, use [NS]Object
	Class super = root_class;
	Class superMeta = root_class;
	if (NULL != cls->super_class)
	{
		// Resolve the superclass if it isn't already resolved
		super = (Class)objc_getClass((char*)cls->super_class);
		if (!objc_test_class_flag(super, objc_class_flag_resolved))
		{
			objc_resolve_class(super);
		}
		superMeta = super->isa;
		// Set the superclass pointer for the class and the superclass
		cls->super_class = super;
		cls->isa->super_class = super->isa;
	}
	// Don't make the root class a subclass of itself
	if (cls != super)
	{
		// Set up the class links 
		cls->sibling_class = super->subclass_list;
		super->subclass_list = cls;
		// Set up the metaclass links
		cls->isa->sibling_class = superMeta->subclass_list;
		superMeta->subclass_list = cls->isa;
	}
	// Mark this class (and its metaclass) as resolved
	objc_set_class_flag(cls, objc_class_flag_resolved);
	objc_set_class_flag(cls->isa, objc_class_flag_resolved);
	// Fix up the ivar offsets
	__objc_compute_ivar_offsets(cls);
	// Send the +load message, if required
	objc_send_load_message(cls->isa);
	if (_objc_load_callback)
	{
		_objc_load_callback(cls, 0);
	}
	return YES;
}

void __objc_resolve_class_links(void)
{
	LOCK_UNTIL_RETURN(__objc_runtime_mutex);
	Class class = unresolved_class_list;
	while ((Nil != class))
	{
		Class next = class->unresolved_class_next;
		objc_resolve_class(class);
		class = next;
	}
}

// FIXME: Remove this once all uses of it in the runtime have been removed
void __objc_add_class_to_hash(Class class)
{
	Class old_class = class_table_get_safe(class->name);
	// If this class is already in the class table, we don't need to do anything.
	if (Nil != old_class) { return; }

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);
	// Note: The old runtime set the class number here.  Nothing ever used the
	// class number though, so we don't bother.
	class_table_insert (class);
}

/**
 * Loads a class.  This function assumes that the runtime mutex is locked.
 */
void objc_load_class(struct objc_class *class)
{
	// The compiler initialises the super class pointer to the name of the
	// superclass, not the superclass pointer.
	// Note: With the new ABI, the class pointer is public.  We could,
	// therefore, directly reference the superclass from the compiler and make
	// the linker resolve it.  This should be done in the GCC-incompatible ABI.
	const char *superclassName = (char*)class->super_class;

	// Work around a bug in some versions of GCC that don't initialize the
	// class structure correctly.
	class->subclass_list = NULL;

	// Insert the class into the class table
	class_table_insert (class);

	// Register all of the selectors used by this class and its metaclass
	__objc_register_selectors_from_class(class);
	__objc_register_selectors_from_class(class->isa);

	// Set the uninstalled dtable.  The compiler could do this as well.
	class->dtable = __objc_uninstalled_dtable;
	class->isa->dtable = __objc_uninstalled_dtable;

	// If this is a root class, make the class into the metaclass's superclass.
	// This means that all instance methods will be available to the class.  
	if (NULL == superclassName)
	{
		class->isa->super_class = class;
	}

	if (class->protocols)
	{
		__objc_init_protocols (class->protocols);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////////////

int objc_getClassList(Class *buffer, int bufferLen)
{
	if (buffer == NULL)
	{
		return class_table->table_used;
	}
	int count = 0;
	struct class_table_internal_table_enumerator *e = NULL;
	Class next;
	while (count < bufferLen &&
		(next = class_table_internal_next(class_table, &e)))
	{
		buffer[count++] = next;
	}
	return count;
}

Class class_getSuperclass(Class cls)
{
	if (!objc_test_class_flag(cls, objc_class_flag_resolved))
	{
		objc_resolve_class(cls);
	}
	return cls->super_class;
}


id objc_getClass(const char *name)
{
	id class = (id)class_table_get_safe(name);

	if (nil != class) { return class; }

	if (0 != _objc_lookup_class)
	{
		class = (id)_objc_lookup_class(name);
	}

	return class;
}

id objc_lookUpClass(const char *name)
{
	return (id)class_table_get_safe(name);
}


id objc_getMetaClass(const char *name)
{
	Class cls = (Class)objc_getClass(name);
	return cls == Nil ? nil : (id)cls->isa;
}

// Legacy interface compatibility

id objc_get_class(const char *name)
{
	return objc_getClass(name);
}

id objc_lookup_class(const char *name)
{
	return objc_getClass(name);
}

id objc_get_meta_class(const char *name)
{
	return objc_getMetaClass(name);
}

Class objc_next_class(void **enum_state)
{
  return class_table_next ( enum_state);
}

Class class_pose_as(Class impostor, Class super_class)
{
	fprintf(stderr, "Class posing is no longer supported.\n");
	fprintf(stderr, "Please use class_replaceMethod() instead.\n");
	abort();
}
