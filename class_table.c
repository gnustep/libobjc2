#include "magic_objects.h"
#include "objc/objc-api.h"
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
	if (!CLS_ISRESOLV(class))
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
	if (CLS_ISRESOLV(cls)) { return; }
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
		root_class = objc_get_class(ROOT_OBJECT_CLASS_NAME);
		if (!CLS_ISRESOLV(root_class))
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
		super = objc_get_class((char*)cls->super_class);
		if (!CLS_ISRESOLV(super))
		{
			objc_resolve_class(super);
		}
		superMeta = super->class_pointer;
		// Set the superclass pointer for the class and the superclass
		cls->super_class = super;
		cls->class_pointer->super_class = super->class_pointer;
	}
	// Set up the class links 
	cls->sibling_class = super->subclass_list;
	super->subclass_list = cls;
	// Set up the metaclass links
	cls->class_pointer->sibling_class = superMeta->subclass_list;
	superMeta->subclass_list = cls->class_pointer;
	// Mark this class (and its metaclass) as resolved
	CLS_SETRESOLV(cls);
	CLS_SETRESOLV(cls->class_pointer);
}

void __objc_resolve_class_links(void)
{
	LOCK(__objc_runtime_mutex);
	Class class;
	while ((class = unresolved_class_list))
	{
		objc_resolve_class(class);
	}
	UNLOCK(__objc_runtime_mutex);
}

int objc_getClassList(Class *buffer, int bufferLen)
{
	if (buffer == NULL)
	{
		return class_table->table_used;
	}
	int count = 0;
	struct class_table_internal_table_enumerator *e;
	Class next;
	while (count < bufferLen &&
		(next = class_table_internal_next(class_table, &e)))
	{
		buffer[count++] = next;
	}
	return count;
}

