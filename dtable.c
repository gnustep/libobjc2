#include <stdio.h>
#include "objc/runtime.h"
#include "sarray2.h"
#include "selector.h"
#include "class.h"
#include "lock.h"
#include "method_list.h"
#include "slot_pool.h"
#include "dtable.h"

SparseArray *__objc_uninstalled_dtable;

/** Head of the list of temporary dtables.  Protected by initialize_lock. */
InitializingDtable *temporary_dtables;
mutex_t initialize_lock;

static uint32_t dtable_depth = 8;

void __objc_init_dispatch_tables ()
{
	INIT_LOCK(initialize_lock);
	__objc_uninstalled_dtable = SparseArrayNewWithDepth(dtable_depth);
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
		SparseArrayInsert(sarray, PTR_TO_IDX(list->methods[i].selector->name),
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
	uint32_t sel_id = PTR_TO_IDX(method->selector->name);
	struct objc_slot *slot = SparseArrayLookup(dtable, sel_id);
	if (NULL != slot)
	{
		// If this method is the one already installed, pretend to install it again.
		if (slot->method == method->imp) { return NO; }

		// If the existing slot is for this class, we can just replace the
		// implementation.  We don't need to bump the version; this operation
		// updates cached slots, it doesn't invalidate them.  
		if (slot->owner == owner)
		{
			// Don't replace methods if we're not meant to (if they're from
			// later in a method list, for example)
			if (!replaceExisting) { return NO; }
			//fprintf(stderr, "Replacing method %p %s in %s with %x\n", slot->method, sel_get_name(method->selector), class->name, method->imp);
			slot->method = method->imp;
			return YES;
		}

		// Check whether the owner of this method is a subclass of the one that
		// owns this method.  If it is, then we don't want to install this
		// method irrespective of other cases, because it has been overridden.
		for (Class installedFor = slot->owner ;
				Nil != installedFor ;
				installedFor = installedFor->super_class)
		{
			if (installedFor == owner) { 
		//fprintf(stderr, "Not installing %s from %s in %s - already overridden from %s\n", sel_get_name(method->selector), owner->name, class->name, slot->owner->name);
				return NO; }
		}
	}
	struct objc_slot *oldSlot = slot;
	//fprintf(stderr, "Installing method %p (%d) %s in %s (previous slot owned by %s)\n", method->imp, sel_id, sel_get_name(method->selector), class->name, slot? oldSlot->owner->name: "");
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

void objc_update_dtable_for_class(Class cls)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }
	//fprintf(stderr, "Updating dtable for %s\n", cls->name);

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);

	SparseArray *methods = SparseArrayNewWithDepth(dtable_depth);
	collectMethodsForMethodListToSparseArray((void*)cls->methods, methods);
	installMethodsInClass(cls, cls, methods, YES);
	// Methods now contains only the new methods for this class.
	mergeMethodsFromSuperclass(cls, cls, methods);
	SparseArrayDestroy(methods);
}
void __objc_update_dispatch_table_for_class(Class cls)
{
	static BOOL warned = NO;
	if (!warned)
	{
		fprintf(stderr, 
			"Warning: Calling deprecated private ObjC runtime function %s\n", __func__);
		warned = YES;
	}
	objc_update_dtable_for_class(cls);
}

static void __objc_install_dispatch_table_for_class(Class class);


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

void objc_resolve_class(Class);

/**
 * Send a +initialize message to the receiver, if required.  
 */
void objc_send_initialize(id object)
{
	Class class = object->isa;
	// If the first message is sent to an instance (weird, but possible and
	// likely for things like NSConstantString, make sure +initialize goes to
	// the class not the metaclass.  
	if (objc_test_class_flag(class, objc_class_flag_meta))
	{
		class = (Class)object;
	}
	Class meta = class->isa;

	// If this class is already initialized (e.g. in another thread), give up.
	if (objc_test_class_flag(class, objc_class_flag_initialized)) { return; }

	// Grab a lock to make sure we are only sending one +initialize message at
	// once.  
	//
	// NOTE: Ideally, we would actually lock on the class object using
	// objc_sync_enter().  This should be fixed once sync.m contains a (fast)
	// special case for classes.
	LOCK_UNTIL_RETURN(&initialize_lock);

	// Make sure that the class is resolved.
	objc_resolve_class(class);

	// Make sure that the superclass is initialized first.
	if (Nil != class->super_class)
	{
		objc_send_initialize((id)class->super_class);
	}

	// Superclass +initialize might possibly send a message to this class, in
	// which case this method would be called again.  See NSObject and
	// NSAutoreleasePool +initialize interaction in GNUstep.
	if (objc_test_class_flag(class, objc_class_flag_initialized)) { return; }

	// Set the initialized flag on both this class and its metaclass, to make
	// sure that +initialize is only ever sent once.
	objc_set_class_flag(class, objc_class_flag_initialized);
	objc_set_class_flag(meta, objc_class_flag_initialized);


	// Create a temporary dtable, to be installed later.
	SparseArray *class_dtable = create_dtable_for_class(class);
	SparseArray *dtable = create_dtable_for_class(meta);

	// Create an entry in the dtable look-aside buffer for this.  When sending
	// a message to this class in future, the lookup function will check this
	// buffer if the receiver's dtable is not installed, and block if
	// attempting to send a message to this class.
	InitializingDtable meta_buffer = { meta, dtable, temporary_dtables };
	InitializingDtable buffer = { class, class_dtable, &meta_buffer };

	// Store the buffer in the temporary dtables list.  Note that it is safe to
	// insert it into a global list, even though it's a temporary variable,
	// because we will clean it up after this function.
	//
	// FIXME: This will actually break if +initialize throws an exception...
	temporary_dtables = &buffer;

	static SEL initializeSel = 0;
	if (0 == initializeSel)
	{
		initializeSel = sel_registerName("initialize");
	}
	struct objc_slot *initializeSlot = 
		SparseArrayLookup(dtable, PTR_TO_IDX(initializeSel->name));

	if (0 != initializeSlot)
	{
		if (Nil != class->super_class)
		{
			// The dtable to use for sending messages to the superclass.  This is
			// the superclass's metaclass' dtable.
			SparseArray *super_dtable = class->super_class->isa->dtable;
			struct objc_slot *superSlot = SparseArrayLookup(super_dtable,
					PTR_TO_IDX(initializeSel->name));
			// Check that this IMP comes from the class, not from its superclass.
			// Note that the superclass dtable is guaranteed to be installed at
			// this point because we sent it a +initialize message already.
			if (0 == superSlot || superSlot->method != initializeSlot->method)
			{
				initializeSlot->method((id)class, initializeSel);
			}
		}
		else
		{
			initializeSlot->method((id)class, initializeSel);
		}
	}

	// Install the real dtable for both the class and the metaclass.
	meta->dtable = dtable;
	class->dtable = class_dtable;

	// Remove the look-aside buffer entry.
	if (temporary_dtables == &buffer)
	{
		temporary_dtables = meta_buffer.next;
	}
	else
	{
		InitializingDtable *prev = temporary_dtables;
		while (prev->next->class != class)
		{
			prev = prev->next;
		}
		prev->next = meta_buffer.next;
	}
}
