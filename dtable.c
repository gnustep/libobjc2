#include <stdio.h>
#include <stdlib.h>
#include "objc/runtime.h"
#include "sarray2.h"
#include "selector.h"
#include "class.h"
#include "lock.h"
#include "method_list.h"
#include "slot_pool.h"
#include "dtable.h"

dtable_t __objc_uninstalled_dtable;

/** Head of the list of temporary dtables.  Protected by initialize_lock. */
InitializingDtable *temporary_dtables;
mutex_t initialize_lock;
static uint32_t dtable_depth = 8;


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
		//fprintf(stderr, "Adding method %s (%d)\n", sel_getName(list->methods[i].selector), PTR_TO_IDX(list->methods[i].selector->name));
		SparseArrayInsert(sarray, list->methods[i].selector->index,
				(void*)&list->methods[i]);
	}
}


#ifdef __OBJC_LOW_MEMORY__

struct objc_dtable
{
	struct cache_line
	{
		uint32_t idx;
		uint32_t version;
		struct objc_slot *slot;
	} cache[8];
	Class cls;
	struct slots_list
	{
		uint32_t idx;
		struct objc_slot *slot;
	} *slots;
	int slot_count;
	int slot_size;
	mutex_t lock;
};

void __objc_init_dispatch_tables ()
{
	INIT_LOCK(initialize_lock);
}

Class class_getSuperclass(Class);

void __objc_update_dispatch_table_for_class(Class cls)
{
	static BOOL warned = NO;
	if (!warned)
	{
		fprintf(stderr, 
			"Warning: Calling deprecated private ObjC runtime function %s\n", __func__);
		warned = YES;
	}
}

static dtable_t create_dtable_for_class(Class class)
{
	// Don't create a dtable for a class that already has one
	if (classHasDtable(class)) { return dtable_for_class(class); }

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);

	// Make sure that another thread didn't create the dtable while we were
	// waiting on the lock.
	if (classHasDtable(class)) { return dtable_for_class(class); }

	Class super = class_getSuperclass(class);

	/* Allocate dtable if necessary */
	dtable_t dtable = calloc(1, sizeof(struct objc_dtable));
	dtable->cls = class;
	INIT_LOCK(dtable->lock);

	return dtable;
}


void objc_resize_dtables(uint32_t newSize)
{
	if (1<<dtable_depth > newSize) { return; }
	dtable_depth <<= 1;
}

#define HASH_UID(uid) ((uid >> 2) & 7)

static struct objc_slot* check_cache(dtable_t dtable, uint32_t uid)
{
	int i = HASH_UID(uid);
	volatile struct cache_line *cache = &dtable->cache[i];
	int32_t initial_idx = cache->idx;

	if (initial_idx != uid)
	{
		return NULL;
	}

	struct objc_slot *slot;
	int32_t idx;
	int32_t version;
	do
	{
		initial_idx = cache->idx;
		version = cache->version;
		slot = cache->slot;
		__sync_synchronize();
		idx = cache->idx;
	} while (idx != initial_idx);

	return (idx == uid) && (slot->version == version) ? slot : NULL;
}

static struct slots_list *find_slot(uint32_t uid, 
		struct slots_list *slots, int slot_count)
{
	if (slot_count == 0) { return NULL; }
	int idx = slot_count >> 1;
	struct slots_list *slot = &slots[idx];
	if (slot_count == 1)
	{
		if (slot->idx == uid)
		{
			return slot;
		}
		return NULL;
	}
	if (slot->idx > uid)
	{
		return find_slot(uid, slots, idx);
	}
	if (slot->idx < uid)
	{
		return find_slot(uid, slots+idx, slot_count - idx);
	}
	if (slot->idx == uid)
	{
		return slot;
	}
	return NULL;
}

static int slot_cmp(const void *l, const void *r)
{
	return (((struct slots_list*)l)->idx - ((struct slots_list*)r)->idx);
}

static void insert_slot(dtable_t dtable, struct objc_slot *slot, uint32_t idx)
{
	if (dtable->slot_size == dtable->slot_count)
	{
		dtable->slot_size += 16;
		dtable->slots = realloc(dtable->slots, dtable->slot_size *
				sizeof(struct slots_list));
		assert(NULL != dtable->slots && "Out of memory!");
	}
	dtable->slots[dtable->slot_count].slot = slot;
	dtable->slots[dtable->slot_count++].idx = idx;
}

static void update_dtable(dtable_t dtable)
{
	//fprintf(stderr, "Updating dtable for %s! %d\n", dtable->cls->name, dtable_depth);
	Class cls = dtable->cls;

	if (NULL == cls->methods) { return; }

	SparseArray *methods = SparseArrayNewWithDepth(dtable_depth);
	collectMethodsForMethodListToSparseArray((void*)cls->methods, methods);

	if (NULL == dtable->slots)
	{
		dtable->slots = calloc(sizeof(struct slots_list), 16);
		dtable->slot_size = 16;
	}

	uint32_t old_slot_count = dtable->slot_count;
	struct objc_method *m;
	uint32_t idx = 0;
	while ((m = SparseArrayNext(methods, &idx)))
	{
		uint32_t idx = m->selector->index;
		struct slots_list *s = find_slot(idx, dtable->slots, old_slot_count);
		if (NULL != s)
		{
			s->slot->method = m->imp;
			s->slot->version++;
		}
		else
		{
			struct objc_slot *slot = new_slot_for_method_in_class(m, cls);
			insert_slot(dtable, slot, idx);
			if (Nil != cls->super_class)
			{
				slot = objc_dtable_lookup(dtable_for_class(cls->super_class), idx);
				if (NULL != slot)
				{
					slot->version++;
				}
			}
		}
	}
	mergesort(dtable->slots, dtable->slot_count, sizeof(struct slots_list),
			slot_cmp);
	SparseArrayDestroy(methods);
}

void objc_update_dtable_for_class(Class cls)
{
	dtable_t dtable = dtable_for_class(cls);
	// Be lazy about constructing the slot list - don't do it unless we actually
	// need to access it
	if ((NULL == dtable) || (NULL == dtable->slots)) { return; }

	LOCK_UNTIL_RETURN(&dtable->lock);

	update_dtable(dtable);

}

struct objc_slot* objc_dtable_lookup(dtable_t dtable, uint32_t uid)
{
	if (NULL == dtable) { return NULL; }

	struct objc_slot *slot = check_cache(dtable, uid);
	
	if (NULL != slot)
	{
		return slot;
	}

	LOCK_UNTIL_RETURN(&dtable->lock);
	if (NULL == dtable->slots)
	{
		update_dtable(dtable);
	}
	//fprintf(stderr, "Using slow path: %d entries in table for %s\n", dtable->slot_count, dtable->cls->name);
	struct slots_list *s = find_slot(uid, dtable->slots, dtable->slot_count);
	if (NULL != s)
	{
		slot = s->slot;
		int i = HASH_UID(uid);
		volatile struct cache_line *cache = &dtable->cache[i];
		cache->idx = 0;
		cache->version = slot->version;
		cache->slot = slot;
		__sync_synchronize();
		cache->idx = uid;
		return slot;
	}

	if (NULL != dtable->cls->super_class)
	{
		return objc_dtable_lookup(dtable->cls->super_class->dtable, uid);
	}
	return NULL;
}
dtable_t objc_copy_dtable_for_class(dtable_t old, Class cls)
{
	dtable_t dtable = calloc(1, sizeof(struct objc_dtable));
	dtable->cls = cls;
	INIT_LOCK(dtable->lock);
	return dtable;
}

#else


void __objc_init_dispatch_tables ()
{
	INIT_LOCK(initialize_lock);
	__objc_uninstalled_dtable = SparseArrayNewWithDepth(dtable_depth);
}

static BOOL installMethodInDtable(Class class,
                                  Class owner,
                                  SparseArray *dtable,
                                  struct objc_method *method,
                                  BOOL replaceExisting)
{
	assert(__objc_uninstalled_dtable != dtable);
	uint32_t sel_id = method->selector->index;
	struct objc_slot *slot = SparseArrayLookup(dtable, sel_id);
	if (NULL != slot)
	{
		//fprintf(stderr, "Slot already exists...\n");
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
			//fprintf(stderr, "Replacing method %p %s in %s with %p\n", slot->method, sel_getName(method->selector), class->name, method->imp);
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
			if (installedFor == owner)
			{
		//fprintf(stderr, "Not installing %s from %s in %s - already overridden from %s\n", sel_getName(method->selector), owner->name, class->name, slot->owner->name);
				return NO;
			}
		}
	}
	struct objc_slot *oldSlot = slot;
	//fprintf(stderr, "Installing method %p (%d) %s in %s (previous slot owned by %s)\n", method->imp, sel_id, sel_getName(method->selector), class->name, slot? oldSlot->owner->name: "(no one)");
	slot = new_slot_for_method_in_class((void*)method, owner);
	SparseArrayInsert(dtable, sel_id, slot);
	// In TDD mode, we also register the first typed method that we
	// encounter as the untyped version.
#ifdef TYPE_DEPENDENT_DISPATCH
	SparseArrayInsert(dtable, get_untyped_idx(method->selector), slot);
#endif
	// Invalidate the old slot, if there is one.
	if (NULL != oldSlot)
	{
		//fprintf(stderr, "Overriding method %p %s from %s in %s with %x\n", slot->method, sel_getName(method->selector), oldSlot->owner->name, class->name, method->imp);
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

	//fprintf(stderr, "Adding methods to %s\n", cls->name);
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

static SparseArray *create_dtable_for_class(Class class)
{
	// Don't create a dtable for a class that already has one
	if (classHasDtable(class)) { return dtable_for_class(class); }

	LOCK_UNTIL_RETURN(__objc_runtime_mutex);

	// Make sure that another thread didn't create the dtable while we were
	// waiting on the lock.
	if (classHasDtable(class)) { return dtable_for_class(class); }

	Class super = class_getSuperclass(class);
	dtable_t dtable;


	if (Nil == super)
	{
		dtable = SparseArrayNewWithDepth(dtable_depth);
	}
	else
	{
		dtable_t super_dtable = dtable_for_class(super);
		if (super_dtable == __objc_uninstalled_dtable)
		{
			super_dtable = create_dtable_for_class(super);
		}
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

dtable_t objc_copy_dtable_for_class(dtable_t old, Class cls)
{
	return SparseArrayCopy(old);
}

#endif // __OBJC_LOW_MEMORY__

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
	dtable_t class_dtable = create_dtable_for_class(class);
	InitializingDtable buffer = { class, class_dtable, temporary_dtables };
	temporary_dtables = &buffer;

	// Create an entry in the dtable look-aside buffer for this.  When sending
	// a message to this class in future, the lookup function will check this
	// buffer if the receiver's dtable is not installed, and block if
	// attempting to send a message to this class.
	dtable_t dtable = create_dtable_for_class(meta);
	InitializingDtable meta_buffer = { meta, dtable, temporary_dtables };
	temporary_dtables = &meta_buffer;

	// Store the buffer in the temporary dtables list.  Note that it is safe to
	// insert it into a global list, even though it's a temporary variable,
	// because we will clean it up after this function.
	//
	// FIXME: This will actually break if +initialize throws an exception...

	static SEL initializeSel = 0;
	if (0 == initializeSel)
	{
		initializeSel = sel_registerName("initialize");
	}
	struct objc_slot *initializeSlot = 
		objc_dtable_lookup(dtable, initializeSel->index);

	if (0 != initializeSlot)
	{
		if (Nil != class->super_class)
		{
			// The dtable to use for sending messages to the superclass.  This
			// is the superclass's metaclass' dtable.
			dtable_t super_dtable = dtable_for_class(class->super_class->isa);
			struct objc_slot *superSlot = objc_dtable_lookup(super_dtable,
					initializeSel->index);
			// Check that this IMP comes from the class, not from its
			// superclass.  We still have to use dtable_for_class() here
			// because our +initialize call might be in response to a message
			// sent from a subclass (e.g. NSObject +initialize sending a
			// message to NSAutoreleasePool: NSObject's dtable won't have been
			// installed at this point.
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
	if (temporary_dtables == &meta_buffer)
	{
		temporary_dtables = buffer.next;
	}
	else
	{
		InitializingDtable *prev = temporary_dtables;
		while (prev->next->class != meta)
		{
			prev = prev->next;
		}
		prev->next = buffer.next;
	}
}
