#include "lock.h"
#include <stdint.h>
#include <dlfcn.h>

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
	// Returning a nil slot allows the caller to cache the lookup for nil too,
	// although this is not particularly useful because the nil method can be
	// inlined trivially.
	if(*receiver == nil)
	{
		return &nil_slot;
	}

	if (__builtin_expect(sender == nil
		||
		(sender->class_pointer->info & (*receiver)->class_pointer->info & _CLS_PLANE_AWARE),1))
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

#ifdef PROFILE
/**
 * When profiling, the runtime writes out two files, one containing tuples of
 * call sites and associated information, the other containing symbolic
 * information for resolving these.  The loggedValues sparse array is used to prevent duplication of 
 */
static struct sarray *loggedValues;
/**
 * Mutex used to protect non-thread-safe parts of the profiling subsystem.
 */
static mutex_t profileLock;
/**
 * File used for writing the profiling symbol table.
 */
FILE *profileSymbols;
/**
 * File used for writing the profiling data.
 */
FILE *profileData;

static char *objc_profile_resolve_symbol_null(void *addr) { return NULL; }
/**
 * Hook allowing JIT'd functions to be resolved.  Takes an address as an
 * argument and returns the symbol name.
 */
char *(*objc_profile_resolve_symbol)(void *addr) =
	objc_profile_resolve_symbol_null;


// Don't enable profiling in the default build (yet)
struct profile_info 
{
	const char *module;
	int32_t callsite;
	IMP method;
	Class cls; 
};

static void __objc_profile_init(void)
{
	INIT_LOCK(profileLock);
	loggedValues = sarray_new(128, 0);
	profileSymbols = fopen("objc_profile.symbols", "a");
	profileData = fopen("objc_profile.data", "a");
	// Write markers indicating a new run.  
	fprintf(profileSymbols, "=== NEW TRACE ===\n");
	struct profile_info profile_data = { 0, 0, 0, 0};
	fwrite(&profile_data, sizeof(profile_data), 1, profileData);
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
	if (NULL == loggedValues)
	{
		LOCK(__objc_runtime_mutex);
		if (NULL == loggedValues)
		{
			__objc_profile_init();
		}
		UNLOCK(__objc_runtime_mutex);
	}
	// Look up the class if the receiver is not nil
	Class cls = Nil;
	if (nil != *receiver)
	{
		cls = (*receiver)->class_pointer;
		if (!sarray_get_safe(loggedValues, (size_t)cls))
		{
			LOCK(&profileLock);
			if (!sarray_get_safe(loggedValues, (size_t)cls))
			{
				fprintf(profileSymbols, "%zx %s\n", (size_t)cls, cls->name);
			}
			UNLOCK(&profileLock);
		}
	}
	Slot_t slot = objc_msg_lookup_sender(receiver, selector, sender);
	IMP method = (IMP)0;
	if (0 != slot->version)
	{
		method = slot->method;
		if (!sarray_get_safe(loggedValues, (size_t)method))
		{
			Dl_info info;
			const char *symbolName;
			if (dladdr((void*)method, &info))
			{
				symbolName = info.dli_sname;
			}
			else
			{
				symbolName = objc_profile_resolve_symbol((void*)method);
			}
			if (NULL != symbolName)
			{
				LOCK(&profileLock);
				if (!sarray_get_safe(loggedValues, (size_t)method))
				{
					fprintf(profileSymbols, "%zx %s\n", (size_t)method, symbolName);
				}
				UNLOCK(&profileLock);
				sarray_at_put_safe(loggedValues, (size_t)method, (void*)1);
			}
		}
	}
	struct profile_info profile_data = { module, callsite, method, cls };
	fwrite(&profile_data, sizeof(profile_data), 1, profileData);
	return slot;
}
#endif
