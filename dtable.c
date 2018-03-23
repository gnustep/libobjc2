#define __BSD_VISIBLE 1
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "objc/runtime.h"
#include "objc/hooks.h"
#include "sarray2.h"
#include "selector.h"
#include "class.h"
#include "lock.h"
#include "method_list.h"
#include "slot_pool.h"
#include "dtable.h"
#include "visibility.h"
#include "asmconstants.h"

_Static_assert(__builtin_offsetof(struct objc_class, dtable) == DTABLE_OFFSET,
		"Incorrect dtable offset for assembly");
_Static_assert(__builtin_offsetof(SparseArray, shift) == SHIFT_OFFSET,
		"Incorrect shift offset for assembly");
_Static_assert(__builtin_offsetof(SparseArray, data) == DATA_OFFSET,
		"Incorrect data offset for assembly");
_Static_assert(__builtin_offsetof(struct objc_slot, method) == SLOT_OFFSET,
		"Incorrect slot offset for assembly");

PRIVATE dtable_t uninstalled_dtable;
#if defined(WITH_TRACING) && defined (__x86_64)
PRIVATE dtable_t tracing_dtable;
#endif
#ifndef ENOTSUP
#	define ENOTSUP -1
#endif

/** Head of the list of temporary dtables.  Protected by initialize_lock. */
PRIVATE InitializingDtable *temporary_dtables;
/** Lock used to protect the temporary dtables list. */
PRIVATE mutex_t initialize_lock;
/** The size of the largest dtable.  This is a sparse array shift value, so is
 * 2^x in increments of 8. */
static uint32_t dtable_depth = 8;

/**
 * Returns YES if the class implements a method for the specified selector, NO
 * otherwise.
 */
static BOOL ownsMethod(Class cls, SEL sel)
{
	struct objc_slot *slot = objc_get_slot2(cls, sel);
	if ((NULL != slot) && (slot->owner == cls))
	{
		return YES;
	}
	return NO;
}


#ifdef DEBUG_ARC_COMPAT
#define ARC_DEBUG_LOG(...) fprintf(stderr, __VA_LIST__)
#else
#define ARC_DEBUG_LOG(...) do {} while(0)
#endif

/**
 * Checks whether the class implements memory management methods, and whether
 * they are safe to use with ARC.
 */
static void checkARCAccessors(Class cls)
{
	static SEL retain, release, autorelease, isARC;
	if (NULL == retain)
	{
		retain = sel_registerName("retain");
		release = sel_registerName("release");
		autorelease = sel_registerName("autorelease");
		isARC = sel_registerName("_ARCCompliantRetainRelease");
	}
	struct objc_slot *slot = objc_get_slot2(cls, retain);
	if ((NULL != slot) && !ownsMethod(slot->owner, isARC))
	{
		ARC_DEBUG_LOG("%s does not support ARC correctly (implements retain)\n", cls->name);
		objc_clear_class_flag(cls, objc_class_flag_fast_arc);
		return;
	}
	slot = objc_get_slot2(cls, release);
	if ((NULL != slot) && !ownsMethod(slot->owner, isARC))
	{
		ARC_DEBUG_LOG("%s does not support ARC correctly (implements release)\n", cls->name);
		objc_clear_class_flag(cls, objc_class_flag_fast_arc);
		return;
	}
	slot = objc_get_slot2(cls, autorelease);
	if ((NULL != slot) && !ownsMethod(slot->owner, isARC))
	{
		ARC_DEBUG_LOG("%s does not support ARC correctly (implements autorelease)\n", cls->name);
		objc_clear_class_flag(cls, objc_class_flag_fast_arc);
		return;
	}
	objc_set_class_flag(cls, objc_class_flag_fast_arc);
}

PRIVATE void checkARCAccessorsSlow(Class cls)
{
	if (cls->dtable != uninstalled_dtable)
	{
		return;
	}
	static SEL retain, release, autorelease, isARC;
	if (NULL == retain)
	{
		retain = sel_registerName("retain");
		release = sel_registerName("release");
		autorelease = sel_registerName("autorelease");
		isARC = sel_registerName("_ARCCompliantRetainRelease");
	}
	if (cls->super_class != Nil)
	{
		checkARCAccessorsSlow(cls->super_class);
	}
	BOOL superIsFast = objc_test_class_flag(cls, objc_class_flag_fast_arc);
	BOOL selfImplementsRetainRelease = NO;
	for (struct objc_method_list *l=cls->methods ; l != NULL ; l= l->next)
	{
		for (int i=0 ; i<l->count ; i++)
		{
			SEL s = l->methods[i].selector;
			if (sel_isEqual(s, retain) ||
			    sel_isEqual(s, release) ||
			    sel_isEqual(s, autorelease))
			{
				selfImplementsRetainRelease = YES;
			}
			else if (sel_isEqual(s, isARC))
			{
				objc_set_class_flag(cls, objc_class_flag_fast_arc);
				return;
			}
		}
	}
	if (superIsFast && ! selfImplementsRetainRelease)
	{
		objc_set_class_flag(cls, objc_class_flag_fast_arc);
	}
}

static void collectMethodsForMethodListToSparseArray(
		struct objc_method_list *list,
		SparseArray *sarray,
		BOOL recurse)
{
	if (recurse && (NULL != list->next))
	{
		collectMethodsForMethodListToSparseArray(list->next, sarray, YES);
	}
	for (unsigned i=0 ; i<list->count ; i++)
	{
		SparseArrayInsert(sarray, list->methods[i].selector->index,
				(void*)&list->methods[i]);
	}
}


PRIVATE void init_dispatch_tables ()
{
	INIT_LOCK(initialize_lock);
	uninstalled_dtable = SparseArrayNewWithDepth(dtable_depth);
#if defined(WITH_TRACING) && defined (__x86_64)
	tracing_dtable = SparseArrayNewWithDepth(dtable_depth);
#endif
}

#if defined(WITH_TRACING) && defined (__x86_64)
static int init;

static void free_thread_stack(void* x)
{
	free(*(void**)x);
}
static pthread_key_t thread_stack_key;
static void alloc_thread_stack(void)
{
	pthread_key_create(&thread_stack_key, free_thread_stack);
	init = 1;
}

PRIVATE void* pushTraceReturnStack(void)
{
	static pthread_once_t once_control = PTHREAD_ONCE_INIT;
	if (!init)
	{
		pthread_once(&once_control, alloc_thread_stack);
	}
	void **stack = pthread_getspecific(thread_stack_key);
	if (stack == 0)
	{
		stack = malloc(4096*sizeof(void*));
	}
	pthread_setspecific(thread_stack_key, stack + 5);
	return stack;
}

PRIVATE void* popTraceReturnStack(void)
{
	void **stack = pthread_getspecific(thread_stack_key);
	stack -= 5;
	pthread_setspecific(thread_stack_key, stack);
	return stack;
}
#endif

int objc_registerTracingHook(SEL aSel, objc_tracing_hook aHook)
{
#if defined(WITH_TRACING) && defined (__x86_64)
	// If this is an untyped selector, register it for every typed variant
	if (sel_getType_np(aSel) == 0)
	{
		SEL buffer[16];
		SEL *overflow = 0;
		int count = sel_copyTypedSelectors_np(sel_getName(aSel), buffer, 16);
		if (count > 16)
		{
			overflow = calloc(count, sizeof(SEL));
			sel_copyTypedSelectors_np(sel_getName(aSel), buffer, 16);
			for (int i=0 ; i<count ; i++)
			{
				SparseArrayInsert(tracing_dtable, overflow[i]->index, aHook);
			}
			free(overflow);
		}
		else
		{
			for (int i=0 ; i<count ; i++)
			{
				SparseArrayInsert(tracing_dtable, buffer[i]->index, aHook);
			}
		}
	}
	SparseArrayInsert(tracing_dtable, aSel->index, aHook);
	return 0;
#else
	return ENOTSUP;
#endif
}

static BOOL installMethodInDtable(Class class,
                                  Class owner,
                                  SparseArray *dtable,
                                  struct objc_method *method,
                                  BOOL replaceExisting)
{
	ASSERT(uninstalled_dtable != dtable);
	uint32_t sel_id = method->selector->index;
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
				return NO;
			}
		}
	}
	struct objc_slot *oldSlot = slot;
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
	assert(uninstalled_dtable != dtable);

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

PRIVATE void objc_update_dtable_for_class(Class cls)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }

	LOCK_RUNTIME_FOR_SCOPE();

	SparseArray *methods = SparseArrayNewWithDepth(dtable_depth);
	collectMethodsForMethodListToSparseArray((void*)cls->methods, methods, YES);
	installMethodsInClass(cls, cls, methods, YES);
	// Methods now contains only the new methods for this class.
	mergeMethodsFromSuperclass(cls, cls, methods);
	SparseArrayDestroy(methods);
	checkARCAccessors(cls);
}

PRIVATE void add_method_list_to_class(Class cls,
                                      struct objc_method_list *list)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }

	LOCK_RUNTIME_FOR_SCOPE();

	SparseArray *methods = SparseArrayNewWithDepth(dtable_depth);
	collectMethodsForMethodListToSparseArray(list, methods, NO);
	installMethodsInClass(cls, cls, methods, YES);
	// Methods now contains only the new methods for this class.
	mergeMethodsFromSuperclass(cls, cls, methods);
	SparseArrayDestroy(methods);
	checkARCAccessors(cls);
}

static dtable_t create_dtable_for_class(Class class, dtable_t root_dtable)
{
	// Don't create a dtable for a class that already has one
	if (classHasDtable(class)) { return dtable_for_class(class); }

	LOCK_RUNTIME_FOR_SCOPE();

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
		if (super_dtable == uninstalled_dtable)
		{
			if (super->isa == class)
			{
				super_dtable = root_dtable;
			}
			else
			{
				abort();
			}
		}
		dtable = SparseArrayCopy(super_dtable);
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

PRIVATE void objc_resize_dtables(uint32_t newSize)
{
	// If dtables already have enough space to store all registered selectors, do nothing
	if (1<<dtable_depth > newSize) { return; }

	LOCK_RUNTIME_FOR_SCOPE();

	if (1<<dtable_depth > newSize) { return; }

	dtable_depth += 8;

	uint32_t oldShift = uninstalled_dtable->shift;
	dtable_t old_uninstalled_dtable = uninstalled_dtable;

	uninstalled_dtable = SparseArrayExpandingArray(uninstalled_dtable, dtable_depth);
#if defined(WITH_TRACING) && defined (__x86_64)
	tracing_dtable = SparseArrayExpandingArray(tracing_dtable, dtable_depth);
#endif
	{
		LOCK_FOR_SCOPE(&initialize_lock);
		for (InitializingDtable *buffer = temporary_dtables ; NULL != buffer ; buffer = buffer->next)
		{
			buffer->dtable = SparseArrayExpandingArray(buffer->dtable, dtable_depth);
		}
	}
	// Resize all existing dtables
	void *e = NULL;
	struct objc_class *next;
	while ((next = class_table_next(&e)))
	{
		if (next->dtable == old_uninstalled_dtable)
		{
			next->dtable = uninstalled_dtable;
			next->isa->dtable = uninstalled_dtable;
			continue;
		}
		if (NULL != next->dtable &&
		    ((SparseArray*)next->dtable)->shift == oldShift)
		{
			next->dtable = SparseArrayExpandingArray((void*)next->dtable, dtable_depth);
			next->isa->dtable = SparseArrayExpandingArray((void*)next->isa->dtable, dtable_depth);
		}
	}
}

PRIVATE dtable_t objc_copy_dtable_for_class(dtable_t old, Class cls)
{
	return SparseArrayCopy(old);
}

PRIVATE void free_dtable(dtable_t dtable)
{
	SparseArrayDestroy(dtable);
}

LEGACY void update_dispatch_table_for_class(Class cls)
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

void objc_resolve_class(Class);

__attribute__((unused)) static void objc_release_object_lock(id *x)
{
	objc_sync_exit(*x);
}
/**
 * Macro that is equivalent to @synchronize, for use in C code.
 */
#define LOCK_OBJECT_FOR_SCOPE(obj) \
	__attribute__((cleanup(objc_release_object_lock)))\
	__attribute__((unused)) id lock_object_pointer = obj;\
	objc_sync_enter(obj);

/**
 * Remove a buffer from an entry in the initializing dtables list.  This is
 * called as a cleanup to ensure that it runs even if +initialize throws an
 * exception.
 */
static void remove_dtable(InitializingDtable* meta_buffer)
{
	LOCK(&initialize_lock);
	InitializingDtable *buffer = meta_buffer->next;
	// Install the dtable:
	meta_buffer->class->dtable = meta_buffer->dtable;
	buffer->class->dtable = buffer->dtable;
	// Remove the look-aside buffer entry.
	if (temporary_dtables == meta_buffer)
	{
		temporary_dtables = buffer->next;
	}
	else
	{
		InitializingDtable *prev = temporary_dtables;
		while (prev->next->class != meta_buffer->class)
		{
			prev = prev->next;
		}
		prev->next = buffer->next;
	}
	UNLOCK(&initialize_lock);
}

/**
 * Send a +initialize message to the receiver, if required.  
 */
PRIVATE void objc_send_initialize(id object)
{
	Class class = classForObject(object);
	// If the first message is sent to an instance (weird, but possible and
	// likely for things like NSConstantString, make sure +initialize goes to
	// the class not the metaclass.  
	if (objc_test_class_flag(class, objc_class_flag_meta))
	{
		class = (Class)object;
	}
	Class meta = class->isa;


	// Make sure that the class is resolved.
	objc_resolve_class(class);

	// Make sure that the superclass is initialized first.
	if (Nil != class->super_class)
	{
		objc_send_initialize((id)class->super_class);
	}

	// Lock the runtime while we're creating dtables and before we acquire any
	// other locks.  This prevents a lock-order reversal when
	// dtable_for_class is called from something holding the runtime lock while
	// we're still holding the initialize lock.  We should ensure that we never
	// acquire the runtime lock after acquiring the initialize lock.
	LOCK_RUNTIME();

	// Superclass +initialize might possibly send a message to this class, in
	// which case this method would be called again.  See NSObject and
	// NSAutoreleasePool +initialize interaction in GNUstep.
	if (objc_test_class_flag(class, objc_class_flag_initialized))
	{
		// We know that initialization has started because the flag is set.
		// Check that it's finished by grabbing the class lock.  This will be
		// released once the class has been fully initialized. The runtime
		// lock needs to be released first to prevent a deadlock between the
		// runtime lock and the class-specific lock.
		UNLOCK_RUNTIME();

		objc_sync_enter((id)meta);
		objc_sync_exit((id)meta);
		assert(dtable_for_class(class) != uninstalled_dtable);
		return;
	}

	LOCK_OBJECT_FOR_SCOPE((id)meta);
	LOCK(&initialize_lock);
	if (objc_test_class_flag(class, objc_class_flag_initialized))
	{
		UNLOCK(&initialize_lock);
		UNLOCK_RUNTIME();
		return;
	}
	BOOL skipMeta = objc_test_class_flag(meta, objc_class_flag_initialized);

	// Set the initialized flag on both this class and its metaclass, to make
	// sure that +initialize is only ever sent once.
	objc_set_class_flag(class, objc_class_flag_initialized);
	objc_set_class_flag(meta, objc_class_flag_initialized);

	dtable_t class_dtable = create_dtable_for_class(class, uninstalled_dtable);
	dtable_t dtable = skipMeta ? 0 : create_dtable_for_class(meta, class_dtable);
	// Now we've finished doing things that may acquire the runtime lock, so we
	// can hold onto the initialise lock to make anything doing
	// dtable_for_class block until we've finished updating temporary dtable
	// lists.
	// If another thread holds the runtime lock, it can now proceed until it
	// gets into a dtable_for_class call, and then block there waiting for us
	// to finish setting up the temporary dtable.
	UNLOCK_RUNTIME();

	static SEL initializeSel = 0;
	if (0 == initializeSel)
	{
		initializeSel = sel_registerName("initialize");
	}

	struct objc_slot *initializeSlot = skipMeta ? 0 :
			objc_dtable_lookup(dtable, initializeSel->index);

	// If there's no initialize method, then don't bother installing and
	// removing the initialize dtable, just install both dtables correctly now
	if (0 == initializeSlot)
	{
		if (!skipMeta)
		{
			meta->dtable = dtable;
		}
		class->dtable = class_dtable;
		checkARCAccessors(class);
		UNLOCK(&initialize_lock);
		return;
	}



	// Create an entry in the dtable look-aside buffer for this.  When sending
	// a message to this class in future, the lookup function will check this
	// buffer if the receiver's dtable is not installed, and block if
	// attempting to send a message to this class.
	InitializingDtable buffer = { class, class_dtable, temporary_dtables };
	__attribute__((cleanup(remove_dtable)))
	InitializingDtable meta_buffer = { meta, dtable, &buffer };
	temporary_dtables = &meta_buffer;
	// We now release the initialize lock.  We'll reacquire it later when we do
	// the cleanup, but at this point we allow other threads to get the
	// temporary dtable and call +initialize in other threads.
	UNLOCK(&initialize_lock);
	// We still hold the class lock at this point.  dtable_for_class will block
	// there after acquiring the temporary dtable.

	checkARCAccessors(class);

	// Store the buffer in the temporary dtables list.  Note that it is safe to
	// insert it into a global list, even though it's a temporary variable,
	// because we will clean it up after this function.
	initializeSlot->method((id)class, initializeSel);
}

