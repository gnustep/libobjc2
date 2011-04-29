#include "objc/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "class.h"
#include "properties.h"
#include "spinlock.h"
#include "visibility.h"
#include "nsobject.h"

PRIVATE int spinlocks[spinlock_count];

/**
 * Public function for getting a property.  
 */
id objc_getProperty(id obj, SEL _cmd, ptrdiff_t offset, BOOL isAtomic)
{
	if (nil == obj) { return nil; }
	char *addr = (char*)obj;
	addr += offset;
	id ret;
	if (isAtomic)
	{
		int *lock = lock_for_pointer(addr);
		lock_spinlock(lock);
		ret = *(id*)addr;
		ret = [ret retain];
		unlock_spinlock(lock);
	}
	else
	{
		ret = *(id*)addr;
		ret = [ret retain];
	}
	return [ret autorelease];
}

void objc_setProperty(id obj, SEL _cmd, ptrdiff_t offset, id arg, BOOL isAtomic, BOOL isCopy)
{
	if (nil == obj) { return; }
	if (isCopy)
	{
		arg = [arg copy];
	}
	else
	{
		arg = [arg retain];
	}
	char *addr = (char*)obj;
	addr += offset;
	id old;
	if (isAtomic)
	{
		int *lock = lock_for_pointer(addr);
		lock_spinlock(lock);
		old = *(id*)addr;
		*(id*)addr = arg;
		unlock_spinlock(lock);
	}
	else
	{
		old = *(id*)addr;
		*(id*)addr = arg;
	}
	[old release];
}

/**
 * Structure copy function.  This is provided for compatibility with the Apple
 * APIs (it's an ABI function, so it's semi-public), but it's a bad design so
 * it's not used.  The problem is that it does not identify which of the
 * pointers corresponds to the object, which causes some excessive locking to
 * be needed.
 */
void objc_copyPropertyStruct(void *dest,
                             void *src,
                             ptrdiff_t size,
                             BOOL atomic,
                             BOOL strong)
{
	if (atomic)
	{
		int *lock = lock_for_pointer(src);
		int *lock2 = lock_for_pointer(src);
		lock_spinlock(lock);
		lock_spinlock(lock2);
		memcpy(dest, src, size);
		unlock_spinlock(lock);
		unlock_spinlock(lock2);
	}
	else
	{
		memcpy(dest, src, size);
	}
}

/**
 * Get property structure function.  Copies a structure from an ivar to another
 * variable.  Locks on the address of src.
 */
void objc_getPropertyStruct(void *dest,
                            void *src,
                            ptrdiff_t size,
                            BOOL atomic,
                            BOOL strong)
{
	if (atomic)
	{
		int *lock = lock_for_pointer(src);
		lock_spinlock(lock);
		memcpy(dest, src, size);
		unlock_spinlock(lock);
	}
	else
	{
		memcpy(dest, src, size);
	}
}

/**
 * Set property structure function.  Copes a structure to an ivar.  Locks on
 * dest.
 */
void objc_setPropertyStruct(void *dest,
                            void *src,
                            ptrdiff_t size,
                            BOOL atomic,
                            BOOL strong)
{
	if (atomic)
	{
		int *lock = lock_for_pointer(dest);
		lock_spinlock(lock);
		memcpy(dest, src, size);
		unlock_spinlock(lock);
	}
	else
	{
		memcpy(dest, src, size);
	}
}


objc_property_t class_getProperty(Class cls, const char *name)
{
	// Old ABI classes don't have declared properties
	if (Nil == cls || !objc_test_class_flag(cls, objc_class_flag_new_abi))
	{
		return NULL;
	}
	struct objc_property_list *properties = cls->properties;
	while (NULL != properties)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			objc_property_t p = &properties->properties[i];
			if (strcmp(p->name, name) == 0)
			{
				return p;
			}
		}
		properties = properties->next;
	}
	return NULL;
}
objc_property_t* class_copyPropertyList(Class cls, unsigned int *outCount)
{
	if (Nil == cls || !objc_test_class_flag(cls, objc_class_flag_new_abi))
	{
		if (NULL != outCount) { *outCount = 0; }
		return NULL;
	}
	struct objc_property_list *properties = cls->properties;
	unsigned int count = 0;
	for (struct objc_property_list *l=properties ; NULL!=l ; l=l->next)
	{
		count += l->count;
	}
	if (NULL != outCount)
	{
		*outCount = count;
	}
	if (0 == count)
	{
		return NULL;
	}
	objc_property_t *list = calloc(sizeof(objc_property_t), count);
	unsigned int out = 0;
	for (struct objc_property_list *l=properties ; NULL!=l ; l=l->next)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			list[out] = &l->properties[i];
		}
	}
	return list;
}

const char *property_getName(objc_property_t property)
{
	if (NULL == property) { return NULL; }

	const char *name = property->name;
	if (name[0] == 0)
	{
		name += name[1];
	}
	return name;
}

PRIVATE size_t lengthOfTypeEncoding(const char *types);

const char *property_getAttributes(objc_property_t property)
{
	if (NULL == property) { return NULL; }

	const char *name = (char*)property->name;
	if (name[0] == 0)
	{
		return name + 2;
	}

	size_t typeSize = lengthOfTypeEncoding(property->getter_types);
	size_t nameSize = strlen(property->name);
	// Encoding is T{type},V{name}, so 4 bytes for the "T,V" that we always
	// need.  We also need two bytes for the leading null and the length.
	size_t encodingSize = typeSize + nameSize + 6;
	char flags[16];
	size_t i = 0;
	// Flags that are a comma then a character
	if ((property->attributes & OBJC_PR_readonly) == OBJC_PR_readonly)
	{
		flags[i++] = ',';
		flags[i++] = 'R';
	}
	if ((property->attributes & OBJC_PR_copy) == OBJC_PR_copy)
	{
		flags[i++] = ',';
		flags[i++] = 'C';
	}
	if ((property->attributes & OBJC_PR_retain) == OBJC_PR_retain)
	{
		flags[i++] = ',';
		flags[i++] = '&';
	}
	if ((property->attributes & OBJC_PR_nonatomic) == OBJC_PR_nonatomic)
	{
		flags[i++] = ',';
		flags[i++] = 'N';
	}
	encodingSize += i;
	flags[i] = '\0';
	size_t setterLength = 0;
	size_t getterLength = 0;
	if ((property->attributes & OBJC_PR_getter) == OBJC_PR_getter)
	{
		getterLength = strlen(property->getter_name);
		encodingSize += 2 + getterLength;
	}
	if ((property->attributes & OBJC_PR_setter) == OBJC_PR_setter)
	{
		setterLength = strlen(property->setter_name);
		encodingSize += 2 + setterLength;
	}
	unsigned char *encoding = malloc(encodingSize);
	// Set the leading 0 and the offset of the name
	unsigned char *insert = encoding;
	*(insert++) = 0;
	*(insert++) = 0;
	// Set the type encoding
	*(insert++) = 'T';
	memcpy(insert, property->getter_types, typeSize);
	insert += typeSize;
	// Set the flags
	memcpy(insert, flags, i);
	insert += i;
	if ((property->attributes & OBJC_PR_getter) == OBJC_PR_getter)
	{
		*(insert++) = ',';
		*(insert++) = 'G';
		memcpy(insert, property->getter_name, getterLength);
		insert += getterLength;
	}
	if ((property->attributes & OBJC_PR_setter) == OBJC_PR_setter)
	{
		*(insert++) = ',';
		*(insert++) = 'S';
		memcpy(insert, property->setter_name, setterLength);
		insert += setterLength;
	}
	*(insert++) = ',';
	*(insert++) = 'V';
	encoding[1] = (unsigned char)(uintptr_t)(insert - encoding);
	memcpy(insert, property->name, nameSize);
	insert += nameSize;
	*insert = '\0';
	// If another thread installed the encoding string while we were computing
	// it, then discard the one that we created and return theirs.
	if (!__sync_bool_compare_and_swap(&(property->name), name, encoding))
	{
		free(encoding);
		return property->name + 2;
	}
	return (const char*)(encoding + 2);
}

