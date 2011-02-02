#include "lock.h"
#include "class.h"
#include "sarray2.h"
#include "objc/slot.h"
#include <stdint.h>

#ifdef __OBJC_LOW_MEMORY__
typedef struct objc_dtable* dtable_t;
struct objc_slot* objc_dtable_lookup(dtable_t dtable, uint32_t uid);
#else
typedef SparseArray* dtable_t;
#	define objc_dtable_lookup SparseArrayLookup
#endif

/**
 * Pointer to the sparse array representing the pretend (uninstalled) dtable.
 */
extern dtable_t __objc_uninstalled_dtable;
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
	dtable_t dtable;
	/** The next uninstalled dtable in the list. */
	struct _InitializingDtable *next;
} InitializingDtable;

/** Head of the list of temporary dtables.  Protected by initialize_lock. */
extern InitializingDtable *temporary_dtables;
extern mutex_t initialize_lock;

/**
 * Returns whether a class has an installed dtable.
 */
static inline int classHasInstalledDtable(struct objc_class *cls)
{
	return (cls->dtable != __objc_uninstalled_dtable);
}

/**
 * Returns the dtable for a given class.  If we are currently in an +initialize
 * method then this will block if called from a thread other than the one
 * running the +initialize method.  
 */
static inline dtable_t dtable_for_class(Class cls)
{
	if (classHasInstalledDtable(cls))
	{
		return cls->dtable;
	}
	LOCK_UNTIL_RETURN(&initialize_lock);
	if (classHasInstalledDtable(cls))
	{
		return cls->dtable;
	}
	/* This is a linear search, and so, in theory, could be very slow.  It is
	* O(n) where n is the number of +initialize methods on the stack.  In
	* practice, this is a very small number.  Profiling with GNUstep showed that
	* this peaks at 8. */
	dtable_t dtable = __objc_uninstalled_dtable;
	InitializingDtable *buffer = temporary_dtables;
	while (NULL != buffer)
	{
		if (buffer->class == cls)
		{
			dtable = buffer->dtable;
			break;
		}
		buffer = buffer->next;
	}
	if (dtable == 0)
	{
		dtable = __objc_uninstalled_dtable;
	}
	return dtable;
}

/**
 * Returns whether a class has had a dtable created.  The dtable may be
 * installed, or stored in the look-aside buffer.
 */
static inline int classHasDtable(struct objc_class *cls)
{
	return (dtable_for_class(cls) != __objc_uninstalled_dtable);
}

/**
 * Updates the dtable for a class and its subclasses.  Must be called after
 * modifying a class's method list.
 */
void objc_update_dtable_for_class(Class);

/**
 * Creates a copy of the class's dispatch table.
 */
dtable_t objc_copy_dtable_for_class(dtable_t old, Class cls);
