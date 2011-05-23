/**
 * objc-auto.h - This file provides the interface for Objective-C garbage
 * collection
 */

/**
 * Flags passed to objc_collect.  The low 2 bits specify the type of collection
 * to perform, the remainder provide additional options.
 */
enum
{
	/**
	 * Perform an incremental collection if the collection ratio has not been
	 * exceeded, or a full collection if it has.
	 */
	OBJC_RATIO_COLLECTION        = 0,
	/**
	 * Performs an incremental collection. 
	 */
	OBJC_GENERATIONAL_COLLECTION = 1,
	/**
	 * Performs a full collection.
	 */
	OBJC_FULL_COLLECTION         = 2,
	/**
	 * Repeatedly performs a full collection until collection does not find any
	 * new free memory.
	 */
	OBJC_EXHAUSTIVE_COLLECTION   = 3,
	/**
	 * Only runs the collector (in any mode) if the number of bytes allocated
	 * since the last collection is greater than the threshold.
	 */
	OBJC_COLLECT_IF_NEEDED       = (1 << 3),
	/**
	 * Does not return until the collector has finished running.  
	 */
	OBJC_WAIT_UNTIL_DONE         = (1 << 4),
};

/**
 * Options for objc_clear_stack().
 */
enum
{
	/** Ignored - provided for OS X compatibility. */
	OBJC_CLEAR_RESIDENT_STACK = 1
};


/**
 * Instructs the garbage collector to run.
 */
void objc_collect(unsigned long options);

/**
 * Returns YES if running in GC mode, NO otherwise.
 */
BOOL objc_collectingEnabled(void);

/**
 * Starts concurrent collection.  This is currently unimplemented.
 */
void objc_startCollectorThread(void);

/**
 * Causes all finalizers to be run on the main thread.  This is currently
 * unimplemented.
 */
void objc_finalizeOnMainThread(Class cls);

/**
 * Attempts to delete pointers currently stored on unused bits of the stack.  
 */
void objc_clear_stack(unsigned long options);

/**
 * Returns yes if an object has been finalized.  Currently unimplemented.
 */
BOOL objc_is_finalized(void *ptr);

/**
 * Performs an atomic compare and exchange on a pointer value.  Sets the value
 * at objectLocation to replacement, if the current value is predicate.
 */
BOOL objc_atomicCompareAndSwapPtr(id predicate,
                                  id replacement,
                                  volatile id *objectLocation);
/**
 * Performs an atomic compare and exchange on a pointer value.  Sets the value
 * at objectLocation to replacement, if the current value is predicate.
 */
BOOL objc_atomicCompareAndSwapPtrBarrier(id predicate,
                                         id replacement,
                                         volatile id *objectLocation);

/**
 * Performs an atomic compare and exchange on a pointer value.  Sets the value
 * at objectLocation to replacement, if the current value is predicate.
 */
BOOL objc_atomicCompareAndSwapGlobal(id predicate,
                                     id replacement,
                                     volatile id *objectLocation);
/**
 * Performs an atomic compare and exchange on a pointer value.  Sets the value
 * at objectLocation to replacement, if the current value is predicate.
 */
BOOL objc_atomicCompareAndSwapGlobalBarrier(id predicate,
                                            id replacement,
                                            volatile id *objectLocation);
/**
 * Performs an atomic compare and exchange on a pointer value.  Sets the value
 * at objectLocation to replacement, if the current value is predicate.
 */
BOOL objc_atomicCompareAndSwapInstanceVariable(id predicate,
                                               id replacement,
                                               volatile id *objectLocation);
/**
 * Performs an atomic compare and exchange on a pointer value.  Sets the value
 * at objectLocation to replacement, if the current value is predicate.
 */
BOOL objc_atomicCompareAndSwapInstanceVariableBarrier(id predicate,
                                                      id replacement,
                                                      volatile id *objectLocation);

////////////////////////////////////////////////////////////////////////////////
// The next group of functions are intended to be called automatically by the
// compiler.  Normal user code will not call them.
////////////////////////////////////////////////////////////////////////////////

/**
 * Performs a strong assignment.  Stores val in *ptr, ensuring that the
 * assignment is visible to the collector.
 */
id objc_assign_strongCast(id val, id *ptr);

/**
 * Assigns val to the global pointed to by ptr, ensuring that the assignment is
 * visible to the collector.
 */
id objc_assign_global(id val, id *ptr);
/**
 * Assigns val to the instance variable offset bytes from dest.
 */
id objc_assign_ivar(id val, id dest, ptrdiff_t offset);
/**
 * Performs a memmove() operation, ensuring that the copied bytes are always
 * visible to the collector.
 */
void *objc_memmove_collectable(void *dst, const void *src, size_t size);
/**
 * Reads a weak pointer value.  All reads of pointers declared __weak MUST be
 * via this call.
 */
id objc_read_weak(id *location);
/**
 * Assigns a value to location, which MUST have been declared __weak.  All
 * assignments to weak pointers must go via this function.
 */
id objc_assign_weak(id value, id *location);


/**
 * Registers the current thread with the garbage collector.  Should be done as
 * soon as a thread is created.  Until this is called, the thread's stack will
 * be invisible to the collector.
 */
void objc_registerThreadWithCollector(void);
/**
 * Unregisters the current thread.  The thread's stack becomes invisible to the
 * collector.  This should be called just before the thread exits.
 */
void objc_unregisterThreadWithCollector(void);
/**
 * Registers the current thread with the garbage collector and aborts if the
 * registration failed.
 */
void objc_assertRegisteredThreadWithCollector();

/**
 * Increments the reference count of objects.  This is intended to be used to
 * implement CFRetain().  Reference counts should only be used when storing
 * pointers to objects in untracked allocations (e.g. malloc() memory).
 *
 * This function is intended to be used to implement CFRetain().
 */
id objc_gc_retain(id object);
/**
 * Decrements the reference count on an object.  An object becomes eligible for
 * automatic collection when its reference count reaches zero.  New objects
 * have a reference count of zero, so they are eligible for collection as soon
 * as the last pointer to them vanishes.
 *
 * This function is intended to be used to implement CFRelease().
 */
void objc_gc_release(id object);
/**
 * Allocates a buffer of memory, which will be automatically deallocated by the
 * collector.  If isScanned is true, then this memory may contain pointers.  If
 * not, then pointers stored in the returned region will be ignored.
 *
 * This function is intended to be used to implement NSAllocateCollectable().
 */
void* objc_gc_allocate_collectible(size_t size, BOOL isScanned);

