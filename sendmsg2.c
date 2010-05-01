#include "lock.h"
#include <stdint.h>
#include <dlfcn.h>

#define PROFILE
__thread id objc_msg_sender;

static struct objc_slot nil_slot = { Nil, Nil, "", 1, (IMP)nil_method };

typedef struct objc_slot *Slot_t;

Slot_t objc_msg_lookup_sender(id *receiver, SEL selector, id sender);

// Default implementations of the two new hooks.  Return NULL.
static id objc_proxy_lookup_null(id receiver, SEL op) { return nil; }
static Slot_t objc_msg_forward3_null(id receiver, SEL op) { return &nil_slot; }

id (*objc_proxy_lookup)(id receiver, SEL op) = objc_proxy_lookup_null;
Slot_t (*objc_msg_forward3)(id receiver, SEL op) = objc_msg_forward3_null;

static inline
Slot_t objc_msg_lookup_internal(id *receiver, SEL selector, id sender)
{
	Slot_t result = sarray_get_safe((*receiver)->class_pointer->dtable,
			(sidx)selector->sel_id);
	if (0 == result)
	{
		Class class = (*receiver)->class_pointer;
		struct sarray *dtable = dtable_for_class(class);
		/* Install the dtable if it hasn't already been initialized. */
		if (dtable == __objc_uninstalled_dtable)
		{
			__objc_init_install_dtable (*receiver, selector);
			dtable = dtable_for_class(class);
			result = sarray_get_safe(dtable, (sidx)selector->sel_id);
			if (0 == result)
			{
				objc_mutex_lock(__objc_runtime_mutex);
				dtable = dtable_for_class(class);
				if (dtable == __objc_uninstalled_dtable)
				{
					__objc_install_dispatch_table_for_class(class);
					dtable = dtable_for_class(class);
				}
				objc_mutex_unlock(__objc_runtime_mutex);
				result = sarray_get_safe(dtable, (sidx)selector->sel_id);
			}
		}
		else
		{
			// Check again incase another thread updated the dtable while we
			// weren't looking
			result = sarray_get_safe(dtable, (sidx)selector->sel_id);
		}
		if (0 == result)
		{
			id newReceiver = objc_proxy_lookup(*receiver, selector);
			// If some other library wants us to play forwarding games, try again
			// with the new object.
			if (nil != newReceiver)
			{
				*receiver = newReceiver;
				return objc_msg_lookup_sender(receiver, selector, sender);
			}
			if (0 == result)
			{
				result = objc_msg_forward3(*receiver, selector);
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
		(sender->class_pointer->info & (*receiver)->class_pointer->info & _CLS_PLANE_AWARE),1))
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

Slot_t objc_slot_lookup_super(Super_t super, SEL selector)
{
	id receiver = super->self;
	if (receiver)
	{
		Class class = super->class;
		Slot_t result = sarray_get_safe(class->dtable, (sidx)selector->sel_id);
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

#ifdef PROFILE
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
Slot_t objc_msg_lookup_profile(id *receiver, SEL selector, id sender, 
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
	// Look up the class if the receiver is not nil
	Slot_t slot = objc_msg_lookup_sender(receiver, selector, sender);
	IMP method = slot->method;
	struct profile_info profile_data = { module, callsite, method };
	fwrite(&profile_data, sizeof(profile_data), 1, profileData);
	return slot;
}
#endif
