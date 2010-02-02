/**
 * Modified autorelease pool which performs automatic detection and collection
 * of garbage cycles.
 */
typedef struct _GCThread
{
	/**
	 * Next thread in the list.
	 */
	struct _GCThread *next;
	/**
	 * Last thread in the list.
	 */
	struct _GCThread *last;
	/**
	 * Map of objects that haven't yet escaped from the thread.
	 */
	void *unescapedObjects;
	/**
	 * Top of the stack.
	 */
	void *stackTop;
	/**
	 * Top of the stack.
	 */
	void *stackBottom;
	/**
	 * Per-thread buffer into which objects that are potentially roots in garbage
	 * cycles are stored.
	 */
	id *cycleBuffer;
	/**
	 * Insert point into cycle buffer
	 */
	unsigned int cycleBufferInsert;
	/**
	 * Buffer for objects whose reference count has reached 0.  These may be
	 * freed if there are no references to them on the stack.
	 */
	id *freeBuffer;
	/**
	 * Insert point into to-free buffer
	 */
	unsigned int freeBufferInsert;
	/**
	 * Condition variable prevents the thread from really exiting (and having
	 * its stack deallocated) until the GC thread has removed the thread.
	 */
	void *exitCondition;
	/**
	 * The generation when this stack was last scanned.
	 */
	volatile int scannedInGeneration;
} GCThread;

extern GCThread *GCThreadList;

/**
 * Registers the current thread for garbage collection.
 */
void GCRegisterThread(void);
/**
 * Adds an object for tracing or cycle detection or tracing.
 */
void GCAddObject(id anObject);
/**
 * Drains the objects queued for (potential) collection on the current thread.
 * Passing YES as the argument forces a full sweep of the heap-allocated traced
 * regions.
 *
 * Note that this method performs the collection in a second thread, so some
 * objects may not be collected until after it has run.
 */
void GCDrain(BOOL forceCollect);
