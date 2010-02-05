#include "../objc/objc-api.h"
#include "../objc/runtime.h"
#import "visit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/**
 * Structure storing information about object children.
 *
 * Note: This structure is quite inefficient.  We can optimise it a lot later,
 * if required.
 */
struct GCChildInfo
{
	/** Number of children of this class. */
	unsigned int count;
	/** Offsets of children. */
	size_t *offsets;
	/** Method pointer for enumerating extra children. */
	IMP extraChildren;
};

@interface NSObject
- (BOOL)instancesRespondToSelector: (SEL)aSel;
- (IMP)instanceMethodForSelector: (SEL)aSel;
- (void)_visitChildrenWithFunction: (visit_function_t)function
                           context: (void*)context
                         visitWeak: (BOOL)aFlag;
@end
static SEL visitSelector = @selector(_visitChildrenWithFunction:context:visitWeak:);

/**
 * Macro for adding an offset to the offset buffer and resizing it if required.
 */
#define ADD_OFFSET(offset) \
	do {\
		if (found == space)\
		{\
			space *= 2;\
			buffer = realloc(buffer, sizeof(size_t[space]));\
		}\
		buffer[found++] = offset;\
	} while(0)

// Note: If we want to save space we could use char*s and short*s for objects
// less than 2^8 and 2^16 big and add a header indicating this.
/**
 * Create an instance variable map for the specified class.  Inspects the ivars
 * metadata and creates a GCChildInfo structure for the class.  This is cached
 * in the gc_object_type field in the class structure.
 *
 * FIXME: This is a hack.  The compiler should generate this stuff, not the
 * runtime.
 */
struct GCChildInfo *GCMakeIVarMap(Class aClass)
{
	struct GCChildInfo *info = calloc(1, sizeof(struct GCChildInfo));

	unsigned int ivarCount;
	Ivar *ivars = class_copyIvarList(aClass, &ivarCount);

	if (0 == ivarCount)
	{
		info->count = 0;
		info->offsets = NULL;
	}
	else
	{
		unsigned found = 0;
		// First guess - every instance variable is an object
		size_t space = sizeof(size_t[ivarCount]);
		size_t *buffer = malloc(space);

		for (unsigned i=0 ; i<ivarCount ; ++i)
		{
			Ivar ivar = ivars[i];
			const char *type = ivar_getTypeEncoding(ivar);
			switch(type[0])
			{
				case '@':
				{
					// If it's an object, add it to the list.
					// FIXME: Weak ivars
					ADD_OFFSET(ivar_getOffset(ivar));
					break;
				}
				case '[':
				case '{':
				{
					if (strchr(type, '@'))
					{
						//FIXME: Parse structures and arrays correctly
						fprintf(stderr, "Compound type found in class %s, type: %s is "
								"incorrectly handled", class_getName(aClass),
								ivar_getTypeEncoding(ivar));
					}
					break;
				}
			}
		}
		info->count = found;
		info->offsets = realloc(buffer, sizeof(size_t[found]));
	}
	/* FIXME: Use the runtime functions for this
	if ([aClass instancesRespondToSelector: visitSelector])
	{
		info->extraChildren = 
			[aClass instanceMethodForSelector: visitSelector];
	}
	*/
	aClass->gc_object_type = info;
	return info;
}

void GCVisitChildren(id object, visit_function_t function, void *argument, 
		BOOL visitWeakChildren)
{
	Class cls = object->class_pointer;
	while (Nil != cls)
	{
		if (NULL == cls->gc_object_type)
		{
			// FIXME: Locking
			GCMakeIVarMap(cls);
		}
		struct GCChildInfo *info = cls->gc_object_type;
		for (unsigned i=0 ; i<info->count ; ++i)
		{
			id child = *(id*)(((char*)object) + info->offsets[i]);
			if (child != nil)
			{
				BOOL isWeak = (intptr_t)child & 1;
				function(object, argument, isWeak);
			}
		}
		if (NULL != info->extraChildren)
		{
			info->extraChildren(object, visitSelector, function, argument,
					visitWeakChildren);
		}
		cls = cls->super_class;
	}
}
