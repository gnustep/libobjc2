#include "visibility.h"
#include "objc/runtime.h"
#include "gc_ops.h"
#include "class.h"
#include <stdlib.h>
#include <stdio.h>

static id allocate_class(Class cls, size_t extraBytes)
{
	return calloc(cls->instance_size + extraBytes, 1);
}

static void *alloc(size_t size)
{
	return calloc(size, 1);
}

PRIVATE struct gc_ops gc_ops_none = 
{
	.allocate_class = allocate_class,
	.malloc         = alloc,
	.free           = free
};
PRIVATE struct gc_ops *gc = &gc_ops_none;

PRIVATE BOOL isGCEnabled = NO;

#ifndef ENABLE_GC
PRIVATE void enableGC(BOOL exclusive)
{
	fprintf(stderr, "Attempting to enable garbage collection, but your"
			"Objective-C runtime was built without garbage collection"
			"support\n");
	abort();
}
#endif
