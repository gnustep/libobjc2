#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "class.h"
#include "visibility.h"
#include "gc_ops.h"
#include "legacy.h"

ptrdiff_t objc_alignof_type(const char *);
ptrdiff_t objc_sizeof_type(const char *);

PRIVATE void objc_compute_ivar_offsets(Class class)
{
	struct objc_ivar_list_legacy *legacy = NULL;
	if (class->ivars == NULL)
	{
		Class super_class = class_getSuperclass(class);
		if (super_class != Nil)
		{
			class->instance_size = super_class->instance_size;
		}
		return;
	}
	if (class->ivars->size != sizeof(struct objc_ivar))
	{
		fprintf(stderr, "Downgrading ivar struct not yet implemented");
		abort();
	}
	int i = 0;
	/* If this class was compiled with support for late-bound ivars, the
	* instance_size field will contain 0 - {the size of the instance variables
	* declared for just this class}.  The individual instance variable offset
	* fields will then be the offsets from the start of the class, and so must
	* have the size of the parent class prepended. */
	if (class->instance_size <= 0)
	{
		Class super = class_getSuperclass(class);
		long ivar_start = 0;
		if (Nil != super)
		{
			if (super->instance_size <= 0)
			{
				objc_compute_ivar_offsets(super);
			}
			ivar_start = super->instance_size;
		}
		long class_size = 0 - class->instance_size;
		class->instance_size = ivar_start - class->instance_size;
		/* For each instance variable, we add the offset if required (it will be zero
		* if this class is compiled with a static ivar layout).  We then set the
		* value of a global variable to the offset value.  
		*
		* Any class compiled with support for the non-fragile ABI, but not actually
		* using it, will export the ivar offset field as a symbol.
		*
		* Note that using non-fragile ivars breaks @defs().  If you need equivalent
		* functionality, provide an alternative @interface with all variables
		* declared @public.
		*/
		if (class->ivars)
		{
			long cumulative_fudge = 0;
			for (i = 0 ; i < class->ivars->count ; i++)
			{
				struct objc_ivar *ivar = &class->ivars->ivar_list[i];
				// We are going to be allocating an extra word for the reference count
				// in front of the object.  This doesn't matter for aligment most of
				// the time, but if we have an instance variable that is a vector type
				// then we will need to ensure that we are properly aligned again.
				long ivar_size = (i+1 == class->ivars->count)
					? (class_size - *ivar->offset)
					: *class->ivars->ivar_list[i+1].offset - *ivar->offset ;
				// FIXME: use alignment
				*ivar->offset += cumulative_fudge;
				// We only need to do the realignment for things that are
				// bigger than a pointer, and we don't need to do it in GC mode
				// where we don't add any extra padding.
				if (!isGCEnabled && (ivar->align > __alignof__(void*)))
				{
					long offset = ivar_start + *ivar->offset + sizeof(intptr_t);
					long fudge = ivar->align - (offset % ivar->align);
					*ivar->offset += fudge;
					class->instance_size += fudge;
					cumulative_fudge += fudge;
					assert((ivar_start + *ivar->offset + sizeof(intptr_t)) % ivar->align == 0);
				}
				*ivar->offset += ivar_start;
			}
			// If we have a legacy ivar list, update the offset in it too -
			// code from older compilers may access this directly!
			struct objc_class_gsv1* legacy = objc_legacy_class_for_class(class);
			if (legacy)
			{
				for (i = 0 ; i < class->ivars->count ; i++)
				{
					legacy->ivars->ivar_list[i].offset = *class->ivars->ivar_list[i].offset;
				}
			}
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
// Public API functions
////////////////////////////////////////////////////////////////////////////////

void object_setIvar(id object, Ivar ivar, id value)
{
	id *addr = (id*)((char*)object + ivar_getOffset(ivar));
	switch (ivarGetOwnership(ivar))
	{
		case ownership_strong:
			objc_storeStrong(addr, value);
			break;
		case ownership_weak:
			objc_storeWeak(addr, value);
			break;
		case ownership_unsafe:
		case ownership_invalid:
			*addr = value;
			break;
	}
}

Ivar object_setInstanceVariable(id obj, const char *name, void *value)
{
	Ivar ivar = class_getInstanceVariable(object_getClass(obj), name);
	if (ivar_getTypeEncoding(ivar)[0] == '@')
	{
		object_setIvar(obj, ivar, *(id*)value);
	}
	else
	{
		size_t size = objc_sizeof_type(ivar_getTypeEncoding(ivar));
		memcpy((char*)obj + ivar_getOffset(ivar), value, size);
	}
	return ivar;
}

id object_getIvar(id object, Ivar ivar)
{
	id *addr = (id*)((char*)object + ivar_getOffset(ivar));
	switch (ivarGetOwnership(ivar))
	{
		case ownership_strong:
			return objc_retainAutoreleaseReturnValue(*addr);
		case ownership_weak:
			return objc_loadWeak(addr);
			break;
		case ownership_unsafe:
		case ownership_invalid:
			return *addr;
		return nil;
	}
}

Ivar object_getInstanceVariable(id obj, const char *name, void **outValue)
{
	Ivar ivar = class_getInstanceVariable(object_getClass(obj), name);
	if (NULL != outValue)
	{
		*outValue = (((char*)obj) + ivar_getOffset(ivar));
	}
	return ivar;
}
