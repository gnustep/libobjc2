#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "class.h"
#include "ivar.h"
#include "visibility.h"
#include "gc_ops.h"

ptrdiff_t objc_alignof_type(const char *);
ptrdiff_t objc_sizeof_type(const char *);

PRIVATE void objc_compute_ivar_offsets(Class class)
{
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
					? (class_size - ivar->offset)
					: class->ivars->ivar_list[i+1].offset - ivar->offset ;
				ivar->offset += cumulative_fudge;
				// We only need to do the realignment for things that are
				// bigger than a pointer, and we don't need to do it in GC mode
				// where we don't add any extra padding.
				if (!isGCEnabled && (ivar_size > sizeof(void*)))
				{
					long offset = ivar_start + ivar->offset + sizeof(intptr_t);
					// For now, assume that nothing needs to be more than 16-byte aligned.
					// This is not correct for AVX vectors, but we probably
					// can't do anything about that for now (as malloc is only
					// giving us 16-byte aligned memory)
					long fudge = 16 - (offset % 16);
					ivar->offset += fudge;
					class->instance_size += fudge;
					cumulative_fudge += fudge;
					assert((ivar_start + ivar->offset + sizeof(intptr_t)) % 16 == 0);
				}
				ivar->offset += ivar_start;
				/* If we're using the new ABI then we also set up the faster ivar
				* offset variables.
				*/
				if (objc_test_class_flag(class, objc_class_flag_new_abi))
				{
					*(class->ivar_offsets[i]) = ivar->offset;
				}
			}
		}
	}
	else
	{
		if (NULL == class->ivars) { return; }

		Class super = class_getSuperclass(class);
		int start = class->ivars->ivar_list[0].offset;
		/* Quick and dirty test.  If the first ivar comes straight after the last
		* class, then it's fine. */
		if (Nil == super || start == super->instance_size) {return; }

		/* Find the last superclass with at least one ivar. */
		while (NULL == super->ivars) 
		{
			super = class_getSuperclass(super);
		}
		struct objc_ivar *ivar =
			&super->ivars->ivar_list[super->ivars->count-1];

		// Find the end of the last ivar - instance_size contains some padding
		// for alignment.
		int real_end = ivar->offset + objc_sizeof_type(ivar->type);
		// Keep going if the new class starts at the end of the superclass
		if (start == real_end)
		{
			return;
		}
		// The classes don't line up, but don't panic; check that the
		// difference is not just padding for alignment
		int align = objc_alignof_type(class->ivars->ivar_list[0].type);
		if (start > real_end && (start - align) < real_end)
		{
			return;
		}

		/* Panic if this class has an instance variable that overlaps the
		* superclass. */
		fprintf(stderr, 
			"Error: Instance variables in %s overlap superclass %s.  ",
			class->name, super->name);
		fprintf(stderr, 
			"Offset of first instance variable, %s, is %d.  ",
			class->ivars->ivar_list[0].name, start);
		fprintf(stderr, 
			"Last instance variable in superclass, %s, ends at offset %d.  ",
			ivar->name, ivar->offset +
			(int)objc_sizeof_type(ivar->type));
		fprintf(stderr, "This probably means that you are subclassing a"
			"class from a library, which has changed in a binary-incompatible"
			"way.\n");
		abort();
	}
}

typedef enum {
	ownership_invalid,
	ownership_strong,
	ownership_weak,
	ownership_unsafe
} ownership;

ownership ownershipForIvar(Class cls, Ivar ivar)
{
	struct objc_ivar_list *list = cls->ivars;
	if ((list == NULL) || (ivar < list->ivar_list)
          || (ivar >= &list->ivar_list[list->count]))
	{
		// Try the superclass
		if (cls->super_class)
		{
			return ownershipForIvar(cls->super_class, ivar);
		}
		return ownership_invalid;
	}
	if (!objc_test_class_flag(cls, objc_class_flag_new_abi))
	{
		return ownership_unsafe;
	}
	if (cls->abi_version < 1)
	{
		return ownership_unsafe;
	}
	if (objc_bitfield_test(cls->strong_pointers, (ivar - list->ivar_list)))
	{
		return ownership_strong;
	}
	if (objc_bitfield_test(cls->weak_pointers, (ivar - list->ivar_list)))
	{
		return ownership_weak;
	}
	return ownership_unsafe;
}

////////////////////////////////////////////////////////////////////////////////
// Public API functions
////////////////////////////////////////////////////////////////////////////////

void object_setIvar(id object, Ivar ivar, id value)
{
	ownershipForIvar(object_getClass(object), ivar);
	id *addr = (id*)((char*)object + ivar_getOffset(ivar));
	switch (ownershipForIvar(object_getClass(object), ivar))
	{
		case ownership_strong:
			objc_storeStrong(addr, value);
			break;
		case ownership_weak:
			objc_storeWeak(addr, value);
			break;
		case ownership_unsafe:
			*addr = value;
			break;
		case ownership_invalid:
#ifndef NDEBUG
			fprintf(stderr, "Ivar does not belong to this class!\n");
#endif
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
	ownershipForIvar(object_getClass(object), ivar);
	id *addr = (id*)((char*)object + ivar_getOffset(ivar));
	switch (ownershipForIvar(object_getClass(object), ivar))
	{
		case ownership_strong:
			return objc_retainAutoreleaseReturnValue(*addr);
		case ownership_weak:
			return objc_loadWeak(addr);
			break;
		case ownership_unsafe:
			return *addr;
		case ownership_invalid:
#ifndef NDEBUG
			fprintf(stderr, "Ivar does not belong to this class!\n");
#endif
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
