__thread id objc_msg_sender;

static struct objc_slot nil_slot = { Nil, Nil, "", 1, (IMP)nil_method };

typedef struct objc_slot *Slot_t;

Slot_t objc_msg_lookup_sender(id *receiver, SEL selector, id sender);

// Default implementations of the two new hooks.  Return NULL.
static id objc_proxy_lookup_null(id receiver, SEL op) { return nil; }
static Slot_t objc_msg_forward3_null(id receiver, SEL op) { return NULL; }

id (*objc_proxy_lookup)(id receiver, SEL op) = objc_proxy_lookup_null;
Slot_t (*objc_msg_forward3)(id receiver, SEL op) = objc_msg_forward3_null;

static inline
Slot_t objc_msg_lookup_internal(id *receiver, SEL selector, id sender)
{
	Slot_t result = sarray_get_safe((*receiver)->class_pointer->dtable,
			(sidx)selector->sel_id);
	if (0 == result)
	{
		/* Install the dtable if it hasn't already been initialized. */
		if ((*receiver)->class_pointer->dtable == __objc_uninstalled_dtable)
		{
			__objc_init_install_dtable (*receiver, selector);
			result = sarray_get_safe((*receiver)->class_pointer->dtable,
				(sidx)selector->sel_id);
		}
		else
		{
			// Check again incase another thread updated the dtable while we
			// weren't looking
			result = sarray_get_safe((*receiver)->class_pointer->dtable,
					(sidx)selector->sel_id);
		}
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
