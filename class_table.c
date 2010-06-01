#include "magic_objects.h"
#include "objc/runtime.h"
#include "objc/hooks.h"
#include "class.h"
#include "lock.h"
#include <stdlib.h>
#include <assert.h>

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
	class_table = class_table_internal_create(16);
}

void objc_resolve_class(Class cls)
{
	// Skip this if the class is already resolved.
	if (objc_test_class_flag(cls, objc_class_flag_resolved)) { return; }
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

	static Class root_class = Nil;
	if (Nil == root_class)
	{
		root_class = (Class)objc_getClass(ROOT_OBJECT_CLASS_NAME);
		if (!objc_test_class_flag(root_class, objc_class_flag_resolved))
		{
			objc_resolve_class(root_class);
		}
		assert(root_class);
	}

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
}

void __objc_resolve_class_links(void)
{
	LOCK_UNTIL_RETURN(__objc_runtime_mutex);
	Class class;
	while ((class = unresolved_class_list))
	{
		objc_resolve_class(class);
	}
}

// Public API

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
