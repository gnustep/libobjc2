#include <dlfcn.h>

/**
 * Check if an object is in one of the sections that the loader allocated.  If
 * so, it won't have a GCKit header so we just assume that it never needs
 * collecting.
 */
static inline BOOL GCObjectIsDynamic(id obj)
{
	Dl_info i;
	return !dladdr(obj, &i);
}
