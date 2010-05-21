#include "sarray2.h"
#define isa class_pointer
#include "class.h"
#include "lock.h"
#define objc_method_list method_list_new
#define objc_method method_new
#include "method_list.h"

SparseArray *__objc_uninstalled_dtable;

/**
 * Structure for maintaining a linked list of temporary dtables.  When sending
 * an +initialize message to a class, we create a temporary dtables and store
 * it in a linked list.  This is then used when sending other messages to
 * instances of classes in the middle of initialisation.
 */
typedef struct _InitializingDtable
{
	/** The class that owns the dtable. */
	Class class;
	/** The dtable for this class. */
	void *dtable;
	/** The next uninstalled dtable in the list. */
	struct _InitializingDtable *next;
} InitializingDtable;

/** Head of the list of temporary dtables.  Protected by initialize_lock. */
InitializingDtable *temporary_dtables;

static uint32_t dtable_depth = 8;

void __objc_init_dispatch_tables ()
{
	INIT_LOCK(initialize_lock);
	__objc_uninstalled_dtable = SparseArrayNewWithDepth(dtable_depth);
}

static inline int classHasInstalledDtable(struct objc_class *cls)
{
	return ((void*)cls->dtable != __objc_uninstalled_dtable);
}


/**
 * Returns the dtable for a given class.  If we are currently in an +initialize
 * method then this will block if called from a thread other than the one
 * running the +initialize method.  
 */
static inline SparseArray *dtable_for_class(Class cls)
{
	if (classHasInstalledDtable(cls))
	{
		return (SparseArray*)cls->dtable;
	}
	LOCK_UNTIL_RETURN(&initialize_lock);
	if (classHasInstalledDtable(cls))
	{
		return (SparseArray*)cls->dtable;
	}
	/* This is a linear search, and so, in theory, could be very slow.  It is
	* O(n) where n is the number of +initialize methods on the stack.  In
	* practice, this is a very small number.  Profiling with GNUstep showed that
	* this peaks at 8. */
	SparseArray *dtable = __objc_uninstalled_dtable;
	InitializingDtable *buffer = temporary_dtables;
	while (NULL != buffer)
	{
		if (buffer->class == cls)
		{
			dtable = (SparseArray*)buffer->dtable;
			break;
		}
		buffer = buffer->next;
	}
	UNLOCK(&initialize_lock);
	if (dtable == 0)
	{
		dtable = __objc_uninstalled_dtable;
	}
	return dtable;
}

static inline int classHasDtable(struct objc_class *cls)
{
	return (dtable_for_class(cls) != __objc_uninstalled_dtable);
}

static void collectMethodsForMethodListToSparseArray(
		struct objc_method_list *list,
		SparseArray *sarray)
{
	if (NULL != list->next)
	{
		collectMethodsForMethodListToSparseArray(list->next, sarray);
	}
	for (unsigned i=0 ; i<list->count ; i++)
	{
		SparseArrayInsert(sarray, PTR_TO_IDX(list->methods[i].selector->sel_id),
				(void*)&list->methods[i]);
	}
}

static BOOL installMethodInDtable(Class class,
                                  Class owner,
                                  SparseArray *dtable,
                                  struct objc_method *method,
                                  BOOL replaceExisting)
{
	assert(__objc_uninstalled_dtable != dtable);
	uint32_t sel_id = PTR_TO_IDX(method->selector->sel_id);
	struct objc_slot *slot = SparseArrayLookup(dtable, sel_id);
	if (NULL != slot)
	{
		// If this method is the one already installed, pretend to install it again.
		if (slot->method == method->imp) { return NO; }

		if (slot->owner == class)
		{
			if (!replaceExisting) { return NO; }
			//fprintf(stderr, "Replacing method %p %s in %s with %x\n", slot->method, sel_get_name(method->selector), class->name, method->imp);
			slot->method = method->imp;
			slot->version++;
			return YES;
		}
	}
	struct objc_slot *oldSlot = slot;
	//fprintf(stderr, "Installing method %p %s in %s\n", method->imp, sel_get_name(method->selector), class->name);
	slot = new_slot_for_method_in_class((void*)method, owner);
	SparseArrayInsert(dtable, sel_id, slot);
	// Invalidate the old slot, if there is one.
	if (NULL != oldSlot)
	{
		//fprintf(stderr, "Overriding method %p %s from %s in %s with %x\n", slot->method, sel_get_name(method->selector), oldSlot->owner->name, class->name, method->imp);
		oldSlot->version++;
	}
	return YES;
}

static void installMethodsInClass(Class cls,
                                  Class owner,
                                  SparseArray *methods,
                                  BOOL replaceExisting)
{
	SparseArray *dtable = dtable_for_class(cls);
	assert(__objc_uninstalled_dtable != dtable);

	uint32_t idx = 0;
	struct objc_method *m;
	while ((m = SparseArrayNext(methods, &idx)))
	{
		if (!installMethodInDtable(cls, owner, dtable, m, replaceExisting))
		{
			// Remove this method from the list, if it wasn't actually installed
			SparseArrayInsert(methods, idx, 0);
		}
	}
}

static void mergeMethodsFromSuperclass(Class super, Class cls, SparseArray *methods)
{
	for (struct objc_class *subclass=cls->subclass_list ; 
		Nil != subclass ; subclass = subclass->sibling_class)
	{
		// Don't bother updating dtables for subclasses that haven't been
		// initialized yet
		if (!classHasDtable(subclass)) { continue; }

		// Create a new (copy-on-write) array to pass down to children
		SparseArray *newMethods = SparseArrayCopy(methods);
		// Install all of these methods except ones that are overridden in the
		// subclass.  All of the methods that we are updating were added in a
		// superclass, so we don't replace versions registered to the subclass.
		installMethodsInClass(subclass, super, newMethods, YES);
		// Recursively add the methods to the subclass's subclasses.
		mergeMethodsFromSuperclass(super, subclass, newMethods);
		SparseArrayDestroy(newMethods);
	}
}

Class class_getSuperclass(Class);

void __objc_update_dispatch_table_for_class(Class cls)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);

	SparseArray *methods = SparseArrayNewWithDepth(dtable_depth);
	collectMethodsForMethodListToSparseArray((void*)cls->methods, methods);
	installMethodsInClass(cls, cls, methods, YES);
	// Methods now contains only the new methods for this class.
	mergeMethodsFromSuperclass(cls, cls, methods);
	SparseArrayDestroy(methods);
}

static SparseArray *create_dtable_for_class(Class class)
{
	// Don't create a dtable for a class that already has one
	if (classHasDtable(class)) { return dtable_for_class(class); }

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);

	// Make sure that another thread didn't create the dtable while we were
	// waiting on the lock.
	if (classHasDtable(class)) { return dtable_for_class(class); }

	Class super = class_getSuperclass(class);

	if (Nil != super && !classHasInstalledDtable(super))
	{
		__objc_install_dispatch_table_for_class(super);
	}

	SparseArray *dtable;

	/* Allocate dtable if necessary */
	if (Nil == super)
	{
		dtable = SparseArrayNewWithDepth(dtable_depth);
	}
	else
	{
		assert(__objc_uninstalled_dtable != dtable_for_class(super));
		dtable = SparseArrayCopy(dtable_for_class(super));
	}

	// When constructing the initial dtable for a class, we iterate along the
	// method list in forward-traversal order.  The first method that we
	// encounter is always the one that we want to keep, so we instruct
	// installMethodInDtable() not to replace methods that are already
	// associated with this class.
	struct objc_method_list *list = (void*)class->methods;

	while (NULL != list)
	{
		for (unsigned i=0 ; i<list->count ; i++)
		{
			installMethodInDtable(class, class, dtable, &list->methods[i], NO);
		}
		list = list->next;
	}

	return dtable;
}

static void __objc_install_dispatch_table_for_class(Class class)
{
	class->dtable = (void*)create_dtable_for_class(class);
}

Class class_table_next(void **e);

void objc_resize_dtables(uint32_t newSize)
{
	// If dtables already have enough space to store all registered selectors, do nothing
	if (1<<dtable_depth > newSize) { return; }

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);

	dtable_depth <<= 1;

	uint32_t oldMask = __objc_uninstalled_dtable->mask;

	SparseArrayExpandingArray(__objc_uninstalled_dtable);
	// Resize all existing dtables
	void *e = NULL;
	struct objc_class *next;
	while ((next = class_table_next(&e)))
	{
		if (next->dtable != (void*)__objc_uninstalled_dtable && 
			NULL != next->dtable &&
			((SparseArray*)next->dtable)->mask == oldMask)
		{
			SparseArrayExpandingArray((void*)next->dtable);
		}
	}
}


#define sarray_get_safe(x,y) SparseArrayLookup((SparseArray*)x, y)
#undef objc_method_list
#undef objc_method
