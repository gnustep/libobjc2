/**
 * malloc.h - defines allocation and deallocation hooks and functions for GCKit.
 */
#include <string.h>

/**
 * Allocate new memory.
 */
extern void *(*gc_alloc_with_zone)(void *zone, size_t bytes);

/**
 * Free memory allocated by gc_alloc_with_zone().
 */
extern void (*gc_free_with_zone)(void *zone, void *mem);

/**
 * Allocates an instance of a class, optionally with some extra bytes at the
 * end.
 */
id GCAllocateObjectWithZone(Class cls, void *zone, size_t extraBytes);
/**
 * Allocates a buffer of the specified size.  The third parameter indicates
 * whether this this memory should be scanned for untracked references.  This
 * buffer itself will be freed when the last reference to it is lost.  If the
 * scan parameter is set to YES then pointer assignments in this region should
 * not use strong-cast assigns or GCRetain().
 */
void *GCAllocateBufferWithZone(void *zone, size_t size, BOOL scan);

void GCFreeObject(id object);
void GCFreeObjectUnsafe(id object);
