#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "class.h"
#include "properties.h"
#include "spinlock.h"
#include "visibility.h"
#include "nsobject.h"
#include "gc_ops.h"
#include "lock.h"

PRIVATE int spinlocks[spinlock_count];

/**
 * Public function for getting a property.  
 */
id objc_getProperty(id obj, SEL _cmd, ptrdiff_t offset, BOOL isAtomic)
{
	if (nil == obj) { return nil; }
	char *addr = (char*)obj;
	addr += offset;
	if (isGCEnabled)
	{
		return *(id*)addr;
	}
	id ret;
	if (isAtomic)
	{
		volatile int *lock = lock_for_pointer(addr);
		lock_spinlock(lock);
		ret = *(id*)addr;
		ret = objc_retain(ret);
		unlock_spinlock(lock);
		ret = objc_autoreleaseReturnValue(ret);
	}
	else
	{
		ret = *(id*)addr;
		ret = objc_retainAutoreleaseReturnValue(ret);
	}
	return ret;
}

void objc_setProperty(id obj, SEL _cmd, ptrdiff_t offset, id arg, BOOL isAtomic, BOOL isCopy)
{
	if (nil == obj) { return; }
	char *addr = (char*)obj;
	addr += offset;

	if (isGCEnabled)
	{
		if (isCopy)
		{
			arg = [arg copy];
		}
		*(id*)addr = arg;
		return;
	}
	if (isCopy)
	{
		arg = [arg copy];
	}
	else
	{
		arg = objc_retain(arg);
	}
	id old;
	if (isAtomic)
	{
		volatile int *lock = lock_for_pointer(addr);
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
	objc_release(old);
}

void objc_setProperty_atomic(id obj, SEL _cmd, ptrdiff_t offset, id arg)
{
	char *addr = (char*)obj;
	addr += offset;
	arg = objc_retain(arg);
	volatile int *lock = lock_for_pointer(addr);
	lock_spinlock(lock);
	id old = *(id*)addr;
	*(id*)addr = arg;
	unlock_spinlock(lock);
	objc_release(old);
}

void objc_setProperty_atomic_copy(id obj, SEL _cmd, ptrdiff_t offset, id arg)
{
	char *addr = (char*)obj;
	addr += offset;

	arg = [arg copy];
	volatile int *lock = lock_for_pointer(addr);
	lock_spinlock(lock);
	id old = *(id*)addr;
	*(id*)addr = arg;
	unlock_spinlock(lock);
	objc_release(old);
}

void objc_setProperty_nonatomic(id obj, SEL _cmd, ptrdiff_t offset, id arg)
{
	char *addr = (char*)obj;
	addr += offset;
	arg = objc_retain(arg);
	id old = *(id*)addr;
	*(id*)addr = arg;
	objc_release(old);
}

void objc_setProperty_nonatomic_copy(id obj, SEL _cmd, ptrdiff_t offset, id arg)
{
	char *addr = (char*)obj;
	addr += offset;
	id old = *(id*)addr;
	*(id*)addr = [arg copy];
	objc_release(old);
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
		volatile int *lock = lock_for_pointer(src);
		volatile int *lock2 = lock_for_pointer(src);
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
		volatile int *lock = lock_for_pointer(src);
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
		volatile int *lock = lock_for_pointer(dest);
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
			if (strcmp(property_getName(p), name) == 0)
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
			list[out++] = &l->properties[i];
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

/**
 * The compiler stores the type encoding of the getter.  We replace this with
 * the type encoding of the property itself.  We use a 0 byte at the start to
 * indicate that the swap has taken place.
 */
static const char *property_getTypeEncoding(objc_property_t property)
{
	if (NULL == property) { return NULL; }
	if (NULL == property->getter_types) { return NULL; }

	const char *name = property->getter_types;
	if (name[0] == 0)
	{
		return &name[1];
	}
	size_t typeSize = lengthOfTypeEncoding(name);
	char *buffer = malloc(typeSize + 2);
	buffer[0] = 0;
	memcpy(buffer+1, name, typeSize);
	buffer[typeSize+1] = 0;
	if (!__sync_bool_compare_and_swap(&(property->getter_types), name, buffer))
	{
		free(buffer);
	}
	return &property->getter_types[1];
}

PRIVATE const char *constructPropertyAttributes(objc_property_t property,
                                                const char *iVarName)
{
	const char *name = (char*)property->name;
	const char *typeEncoding = property_getTypeEncoding(property);
	size_t typeSize = (NULL == typeEncoding) ? 0 : strlen(typeEncoding);
	size_t nameSize = (NULL == name) ? 0 : strlen(name);
	size_t iVarNameSize = (NULL == iVarName) ? 0 : strlen(iVarName);
	// Encoding is T{type},V{name}, so 4 bytes for the "T,V" that we always
	// need.  We also need two bytes for the leading null and the length.
	size_t encodingSize = typeSize + nameSize + 6;
	char flags[16];
	size_t i = 0;
	// Flags that are a comma then a character
	if ((property->attributes & OBJC_PR_readonly) == OBJC_PR_readonly)
	{
		if (i > 0)
		{
			flags[i++] = ',';
		}
		flags[i++] = 'R';
	}
	if ((property->attributes & OBJC_PR_copy) == OBJC_PR_copy)
	{
		if (i > 0)
		{
			flags[i++] = ',';
		}
		flags[i++] = 'C';
	}
	if ((property->attributes & OBJC_PR_retain) == OBJC_PR_retain)
	{
		if (i > 0)
		{
			flags[i++] = ',';
		}
		flags[i++] = '&';
	}
	if ((property->attributes & OBJC_PR_nonatomic) == OBJC_PR_nonatomic)
	{
		if (i > 0)
		{
			flags[i++] = ',';
		}
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
	if (NULL != iVarName)
	{
		encodingSize += 2 + iVarNameSize;
	}
	unsigned char *encoding = malloc(encodingSize);
	// Set the leading 0 and the offset of the name
	unsigned char *insert = encoding;
	*(insert++) = 0;
	*(insert++) = 0;
	// Set the type encoding
	if (NULL != typeEncoding)
	{
		*(insert++) = 'T';
		memcpy(insert, typeEncoding, typeSize);
		insert += typeSize;
	}
	// Set the flags
	memcpy(insert, flags, i);
	insert += i;
	if ((property->attributes & OBJC_PR_getter) == OBJC_PR_getter)
	{
		if (i > 0)
		{
			*(insert++) = ',';
		}
		i++;
		*(insert++) = 'G';
		memcpy(insert, property->getter_name, getterLength);
		insert += getterLength;
	}
	if ((property->attributes & OBJC_PR_setter) == OBJC_PR_setter)
	{
		if (i > 0)
		{
			*(insert++) = ',';
		}
		i++;
		*(insert++) = 'S';
		memcpy(insert, property->setter_name, setterLength);
		insert += setterLength;
	}
	if (i > 0)
	{
		*(insert++) = ',';
	}
	*(insert++) = 'V';
	// If the instance variable name is the same as the property name, then we
	// use the same string for both, otherwise we write the ivar name in the
	// attributes string and then a null and then the name.
	if (NULL != iVarName)
	{
		memcpy(insert, iVarName, iVarNameSize);
		insert += iVarNameSize;
		*(insert++) = '\0';
	}
	encoding[1] = (unsigned char)(uintptr_t)(insert - encoding);
	memcpy(insert, name, nameSize);
	insert += nameSize;
	*insert = '\0';
	// If another thread installed the encoding string while we were computing
	// it, then discard the one that we created and return theirs.
	if (!__sync_bool_compare_and_swap(&(property->name), name, (char*)encoding))
	{
		free(encoding);
		return property->name + 2;
	}
	return (const char*)(encoding + 2);
}


const char *property_getAttributes(objc_property_t property)
{
	if (NULL == property) { return NULL; }

	const char *name = (char*)property->name;
	if (name[0] == 0)
	{
		return name + 2;
	}
	return constructPropertyAttributes(property, NULL);
}

objc_property_attribute_t *property_copyAttributeList(objc_property_t property,
                                                      unsigned int *outCount)
{
	if (NULL == property) { return NULL; }
	objc_property_attribute_t attrs[10];
	int count = 0;

	const char *types = property_getTypeEncoding(property);
	if (NULL != types)
	{
		attrs[count].name = "T";
		attrs[count].value = types;
		count++;
	}
	if ((property->attributes & OBJC_PR_copy) == OBJC_PR_copy)
	{
		attrs[count].name = "C";
		attrs[count].value = "";
		count++;
	}
	if ((property->attributes & OBJC_PR_retain) == OBJC_PR_retain)
	{
		attrs[count].name = "&";
		attrs[count].value = "";
		count++;
	}
	if ((property->attributes & OBJC_PR_nonatomic) == OBJC_PR_nonatomic)
	{
		attrs[count].name = "N";
		attrs[count].value = "";
		count++;
	}
	if ((property->attributes & OBJC_PR_getter) == OBJC_PR_getter)
	{
		attrs[count].name = "G";
		attrs[count].value = property->getter_name;
		count++;
	}
	if ((property->attributes & OBJC_PR_setter) == OBJC_PR_setter)
	{
		attrs[count].name = "S";
		attrs[count].value = property->setter_name;
		count++;
	}
	attrs[count].name = "V";
	attrs[count].value = property_getName(property);
	count++;

	objc_property_attribute_t *propAttrs = calloc(sizeof(objc_property_attribute_t), count);
	memcpy(propAttrs, attrs, count * sizeof(objc_property_attribute_t));
	if (NULL != outCount)
	{
		*outCount = count;
	}
	return propAttrs;
}

PRIVATE struct objc_property propertyFromAttrs(const objc_property_attribute_t *attributes,
                                               unsigned int attributeCount,
                                               const char **name)
{
	struct objc_property p = { 0 };
	for (unsigned int i=0 ; i<attributeCount ; i++)
	{
		switch (attributes[i].name[0])
		{
			case 'T':
			{
				size_t typeSize = strlen(attributes[i].value);
				char *buffer = malloc(typeSize + 2);
				buffer[0] = 0;
				memcpy(buffer+1, attributes[i].value, typeSize);
				buffer[typeSize+1] = 0;
				p.getter_types = buffer;
				break;
			}
			case 'S':
			{
				p.setter_name = strdup(attributes[i].value);
				break;
			}
			case 'G':
			{
				p.getter_name = strdup(attributes[i].value);
				break;
			}
			case 'V':
			{
				*name = attributes[i].value;
				break;
			}
			case 'C':
			{
				p.attributes |= OBJC_PR_copy;
			}
			case '&':
			{
				p.attributes |= OBJC_PR_retain;
			}
			case 'N':
			{
				p.attributes |= OBJC_PR_nonatomic;
			}
		}
	}
	return p;
}

BOOL class_addProperty(Class cls,
                       const char *name,
                       const objc_property_attribute_t *attributes, 
                       unsigned int attributeCount)
{
	if ((Nil == cls) || (NULL == name) || (class_getProperty(cls, name) != 0)) { return NO; }
	const char *iVarname = NULL;
	struct objc_property p = propertyFromAttrs(attributes, attributeCount, &iVarname);
	// If the iVar name is not the same as the name, then we need to construct
	// the attributes string now, otherwise we can construct it lazily.
	if (iVarname)
	{
		p.name = name;
		constructPropertyAttributes(&p, iVarname);
	}
	else
	{
		p.name = strdup(name);
	}

	struct objc_property_list *l = calloc(1, sizeof(struct objc_property_list)
			+ sizeof(struct objc_property));
	l->count = 1;
	memcpy(&l->properties, &p, sizeof(struct objc_property));
	LOCK_RUNTIME_FOR_SCOPE();
	l->next = cls->properties;
	cls->properties = l;
	return YES;
}

void class_replaceProperty(Class cls,
                           const char *name,
                           const objc_property_attribute_t *attributes,
                           unsigned int attributeCount)
{
	if ((Nil == cls) || (NULL == name)) { return; }
	objc_property_t old = class_getProperty(cls, name);
	if (NULL == old)
	{
		class_addProperty(cls, name, attributes, attributeCount);
		return;
	}
	const char *iVarname = name;
	struct objc_property p = propertyFromAttrs(attributes, attributeCount, &iVarname);
	LOCK_RUNTIME_FOR_SCOPE();
	// If the iVar name is not the same as the name, then we need to construct
	// the attributes string now, otherwise we can construct it lazily.
	if (iVarname != name)
	{
		constructPropertyAttributes(&p, iVarname);
	}
	memcpy(old, &p, sizeof(struct objc_property));
}
char *property_copyAttributeValue(objc_property_t property,
                                  const char *attributeName)
{
	if ((NULL == property) || (NULL == attributeName)) { return NULL; }
	switch (attributeName[0])
	{
		case 'T':
		{
			const char *types = property_getTypeEncoding(property);
			return (NULL == types) ? NULL : strdup(types);
		}
		case 'V':
		{
			return strdup(property_getName(property));
		}
		case 'S':
		{
			return strdup(property->setter_name);
		}
		case 'G':
		{
			return strdup(property->getter_name);
		}
		case 'C':
		{
			return ((property->attributes |= OBJC_PR_copy) == OBJC_PR_copy) ? strdup("") : 0;
		}
		case '&':
		{
			return ((property->attributes |= OBJC_PR_retain) == OBJC_PR_retain) ? strdup("") : 0;
		}
		case 'N':
		{
			return ((property->attributes |= OBJC_PR_nonatomic) == OBJC_PR_nonatomic) ? strdup("") : 0;
		}
	}
	return 0;
}
