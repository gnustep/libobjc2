#include "lock.h"
#include "class.h"
#include "sarray2.h"
/**
 * Pointer to the sparse array representing the pretend (uninstalled) dtable.
 */
extern SparseArray *__objc_uninstalled_dtable;
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
extern InitializingDtable *temporary_dtables;
mutex_t initialize_lock;

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
