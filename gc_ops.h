/**
 * Garbage collection operations.
 */
struct gc_ops
{
	/**
	 * Initialises this collector.
	 */
	void (*init)(void);
	/**
	 * Allocates enough space for a class, followed by some extra bytes.
	 */
	id (*allocate_class)(Class, size_t);
};

/**
 * Enables garbage collection, if it isn't already enabled.
 *
 * If the exclusive flag is set, then this will ensure that all -retain /
 * -release / -autorelease messages become no-ops.
 */
PRIVATE void enableGC(BOOL exclusive);
/**
 * The mode for garbage collection
 */
enum objc_gc_mode
{
	/** This module neither uses, nor supports, garbage collection. */
	GC_None     = 0,
	/**
	 * This module uses garbage collection, but also sends retain / release
	 * messages.  It can be used with or without GC.
	 */
	GC_Optional = 1,
	/**
	 * This module expects garbage collection and will break without it.
	 */
	GC_Required = 2
};

/**
 * The current Objective-C garbage collection mode.
 */
extern enum objc_gc_mode gc_mode;

/**
 * The current set of garbage collector operations to use.
 */
extern struct gc_ops *gc;

extern struct gc_ops gc_ops_boehm;
extern struct gc_ops gc_ops_none;
