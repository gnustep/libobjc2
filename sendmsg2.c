#include "objc/runtime.h"
#include "lock.h"
#include "dtable.h"
#include "selector.h"
#include "loader.h"
#include "objc/hooks.h"
#include <stdint.h>
#include <dlfcn.h>
#include <stdio.h>

void objc_send_initialize(id object);

__thread id objc_msg_sender;

static id nil_method(id self, SEL _cmd) { return nil; }

static struct objc_slot nil_slot = { Nil, Nil, "", 1, (IMP)nil_method };

typedef struct objc_slot *Slot_t;

Slot_t objc_msg_lookup_sender(id *receiver, SEL selector, id sender);

// Default implementations of the two new hooks.  Return NULL.
static id objc_proxy_lookup_null(id receiver, SEL op) { return nil; }
static Slot_t objc_msg_forward3_null(id receiver, SEL op) { return &nil_slot; }

id (*objc_proxy_lookup)(id receiver, SEL op) = objc_proxy_lookup_null;
Slot_t (*__objc_msg_forward3)(id receiver, SEL op) = objc_msg_forward3_null;

static struct objc_slot* objc_selector_type_mismatch(Class cls, SEL
		selector, Slot_t result)
{
	fprintf(stderr, "Calling [%s %c%s] with incorrect signature.  "
			"Method has %s, selector has %s\n",
			cls->name,
			class_isMetaClass(cls) ? '+' : '-',
			sel_getName(selector),
			result->types,
			sel_getType_np(selector));
	return result;
}
struct objc_slot* (*_objc_selector_type_mismatch)(Class cls, SEL
		selector, struct objc_slot *result) = objc_selector_type_mismatch;
static 
// Uncomment for debugging
//__attribute__((noinline))
__attribute__((always_inline))
Slot_t objc_msg_lookup_internal(id *receiver,
                                SEL selector, 
                                id sender)
{
retry:;
	Slot_t result = objc_dtable_lookup((*receiver)->isa->dtable,
			PTR_TO_IDX(selector->name));
	if (0 == result)
	{
		Class class = (*receiver)->isa;
		dtable_t dtable = dtable_for_class(class);
		/* Install the dtable if it hasn't already been initialized. */
		if (dtable == __objc_uninstalled_dtable)
		{
			objc_send_initialize(*receiver);
			dtable = dtable_for_class(class);
			result = objc_dtable_lookup(dtable, PTR_TO_IDX(selector->name));
		}
		else
		{
			// Check again incase another thread updated the dtable while we
			// weren't looking
			result = objc_dtable_lookup(dtable, PTR_TO_IDX(selector->name));
		}
		if (0 == result)
		{
			if (!sel_is_mapped(selector))
			{
				objc_register_selector(selector);
				// This should be a tail call, but GCC is stupid and won't let
				// us tail call an always_inline function.
				goto retry;
			}
			if ((result = objc_dtable_lookup(dtable, get_untyped_idx(selector))))
			{
				return _objc_selector_type_mismatch((*receiver)->isa, selector,
						result);
			}
			id newReceiver = objc_proxy_lookup(*receiver, selector);
			// If some other library wants us to play forwarding games, try
			// again with the new object.
			if (nil != newReceiver)
			{
				*receiver = newReceiver;
				return objc_msg_lookup_sender(receiver, selector, sender);
			}
			if (0 == result)
			{
				result = __objc_msg_forward3(*receiver, selector);
			}
		}
	}
	return result;
}


Slot_t (*objc_plane_lookup)(id *receiver, SEL op, id sender) =
	objc_msg_lookup_internal;

/**
 * New Objective-C lookup function.  This permits the lookup to modify the
 * receiver and also supports multi-dimensional dispatch based on the sender.  
 */
Slot_t objc_msg_lookup_sender(id *receiver, SEL selector, id sender)
{
	//fprintf(stderr, "Looking up slot %s\n", sel_get_name(selector));
	// Returning a nil slot allows the caller to cache the lookup for nil too,
	// although this is not particularly useful because the nil method can be
	// inlined trivially.
	if(*receiver == nil)
	{
		return &nil_slot;
	}

	/*
	 * The self pointer is invalid in some code.  This test is disabled until
	 * we can guarantee that it is not (e.g. with GCKit)
	if (__builtin_expect(sender == nil
		||
		(sender->isa->info & (*receiver)->isa->info & _CLS_PLANE_AWARE),1))
	*/
	{
		return objc_msg_lookup_internal(receiver, selector, sender);
	}
	// If we are in plane-aware code
	void *senderPlaneID = *((void**)sender - 1);
	void *receiverPlaneID = *((void**)receiver - 1);
	if (senderPlaneID == receiverPlaneID)
	{
		//fprintf(stderr, "Intraplane message\n");
		return objc_msg_lookup_internal(receiver, selector, sender);
	}
	return objc_plane_lookup(receiver, selector, sender);
}

Slot_t objc_slot_lookup_super(struct objc_super *super, SEL selector)
{
	id receiver = super->receiver;
	if (receiver)
	{
		Class class = super->class;
		Slot_t result = objc_dtable_lookup(dtable_for_class(class),
				PTR_TO_IDX(selector->name));
		if (0 == result)
		{
			// Dtable should always be installed in the superclass
			assert(dtable_for_class(class) != __objc_uninstalled_dtable);
			result = &nil_slot;
		}
		return result;
	}
	else
	{
		return &nil_slot;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Profiling
////////////////////////////////////////////////////////////////////////////////

/**
 * Mutex used to protect non-thread-safe parts of the profiling subsystem.
 */
static mutex_t profileLock;
/**
 * File used for writing the profiling symbol table.
 */
static FILE *profileSymbols;
/**
 * File used for writing the profiling data.
 */
static FILE *profileData;

struct profile_info 
{
	const char *module;
	int32_t callsite;
	IMP method;
};

static void __objc_profile_init(void)
{
	INIT_LOCK(profileLock);
	profileSymbols = fopen("objc_profile.symbols", "a");
	profileData = fopen("objc_profile.data", "a");
	// Write markers indicating a new run.  
	fprintf(profileSymbols, "=== NEW TRACE ===\n");
	struct profile_info profile_data = { 0, 0, 0 };
	fwrite(&profile_data, sizeof(profile_data), 1, profileData);
}

void objc_profile_write_symbols(char **symbols)
{
	if (NULL == profileData)
	{
		LOCK(__objc_runtime_mutex);
		if (NULL == profileData)
		{
			__objc_profile_init();
		}
		UNLOCK(__objc_runtime_mutex);
	}
	LOCK(&profileLock);
	while(*symbols)
	{
		char *address = *(symbols++);
		char *symbol = *(symbols++);
		fprintf(profileSymbols, "%zx %s\n", (size_t)address, symbol);
	}
	UNLOCK(&profileLock);
	fflush(profileSymbols);
}

/**
 * Profiling version of the slot lookup.  This takes a unique ID for the module
 * and the callsite as extra arguments.  The type of the receiver and the
 * address of the resulting function are then logged to a file.  These can then
 * be used to determine whether adding slot caching is worthwhile, and whether
 * any of the resulting methods should be speculatively inlined.
 */
void objc_msg_profile(id receiver, IMP method,
                      const char *module, int32_t callsite)
{
	// Initialize the logging lazily.  This prevents us from wasting any memory
	// when we are not profiling.
	if (NULL == profileData)
	{
		LOCK(__objc_runtime_mutex);
		if (NULL == profileData)
		{
			__objc_profile_init();
		}
		UNLOCK(__objc_runtime_mutex);
	}
	struct profile_info profile_data = { module, callsite, method };
	fwrite(&profile_data, sizeof(profile_data), 1, profileData);
}

/**
 * Looks up a slot without invoking any forwarding mechanisms
 */
Slot_t objc_get_slot(Class cls, SEL selector)
{
	Slot_t result = objc_dtable_lookup(cls->dtable, PTR_TO_IDX(selector->name));
	if (0 == result)
	{
		void *dtable = dtable_for_class(cls);
		/* Install the dtable if it hasn't already been initialized. */
		if (dtable == __objc_uninstalled_dtable)
		{
			//objc_send_initialize((id)cls);
			dtable = dtable_for_class(cls);
			result = objc_dtable_lookup(dtable, PTR_TO_IDX(selector->name));
		}
		else
		{
			// Check again incase another thread updated the dtable while we
			// weren't looking
			result = objc_dtable_lookup(dtable, PTR_TO_IDX(selector->name));
		}
		if (NULL == result)
		{
			if (!sel_is_mapped(selector))
			{
				objc_register_selector(selector);
				return objc_get_slot(cls, selector);
			}
			if ((result = objc_dtable_lookup(dtable, get_untyped_idx(selector))))
			{
				return _objc_selector_type_mismatch(cls, selector, result);
			}
		}
	}
	return result;
}

////////////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////////////

BOOL class_respondsToSelector(Class cls, SEL selector)
{
	if (0 == selector || 0 == cls) { return NO; }

	return NULL != objc_get_slot(cls, selector);
}

IMP class_getMethodImplementation(Class cls, SEL name)
{
	if ((cls = Nil) || (name == NULL)) { return (IMP)0; }
	Slot_t slot = objc_get_slot(cls, name);
	return NULL != slot ? slot->method : __objc_msg_forward2(nil, name);
}

IMP class_getMethodImplementation_stret(Class cls, SEL name)
{
	return class_getMethodImplementation(cls, name);
}


////////////////////////////////////////////////////////////////////////////////
// Legacy compatibility
////////////////////////////////////////////////////////////////////////////////

/**
 * Legacy message lookup function.
 */
BOOL __objc_responds_to(id object, SEL sel)
{
	return class_respondsToSelector(object->isa, sel);
}

IMP get_imp(Class cls, SEL selector)
{
	return class_getMethodImplementation(cls, selector);
}
/**
 * Legacy message lookup function.  Does not support fast proxies or safe IMP
 * caching.
 */
IMP objc_msg_lookup(id receiver, SEL selector)
{
	if (nil == receiver) { return (IMP)nil_method; }

	id self = receiver;
	Slot_t slot = objc_msg_lookup_internal(&self, selector, nil);
	if (self != receiver)
	{
		slot = __objc_msg_forward3(receiver, selector);
	}
	return slot->method;
}

IMP objc_msg_lookup_super(struct objc_super *super, SEL selector)
{
	return objc_slot_lookup_super(super, selector)->method;
}
/**
 * Message send function that only ever worked on a small subset of compiler /
 * architecture combinations.
 */
void *objc_msg_sendv(void)
{
	fprintf(stderr, "objc_msg_sendv() never worked correctly.  Don't use it.\n");
	abort();
}
