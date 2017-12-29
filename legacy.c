#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "objc/runtime.h"
#include "objc/encoding.h"
#include "ivar.h"
#include "class.h"
#include "loader.h"

static ivar_ownership ownershipForIvar(struct legacy_gnustep_objc_class *cls, int idx)
{
	if (objc_get_class_version_legacy(cls) < 2)
	{
		return ownership_unsafe;
	}
	if (objc_bitfield_test(cls->strong_pointers, idx))
	{
		return ownership_strong;
	}
	if (objc_bitfield_test(cls->weak_pointers, idx))
	{
		return ownership_weak;
	}
	return ownership_unsafe;
}

static struct objc_ivar_list *upgradeIvarList(struct legacy_gnustep_objc_class *cls)
{
	struct objc_ivar_list_legacy *l = cls->ivars;
	if (l == NULL)
	{
		return NULL;
	}
	struct objc_ivar_list *n = calloc(1, sizeof(struct objc_ivar_list) +
			l->count*sizeof(struct objc_ivar));
	n->size = sizeof(struct objc_ivar);
	n->count = l->count;
	for (int i=0 ; i<l->count ; i++)
	{
		int nextOffset = (i+1 < l->count) ? l->ivar_list[i+1].offset : cls->instance_size;
		if (nextOffset < 0)
		{
			nextOffset = -nextOffset;
		}
		const char *type = l->ivar_list[i].type;
		int size = nextOffset - l->ivar_list[i].offset;
		n->ivar_list[i].name = l->ivar_list[i].name;
		n->ivar_list[i].type = type;
		if (objc_test_class_flag_legacy(cls, objc_class_flag_new_abi))
		{
			n->ivar_list[i].offset = cls->ivar_offsets[i];
		}
		else
		{
			n->ivar_list[i].offset = &l->ivar_list[i].offset;
		}
		n->ivar_list[i].align = ((type == NULL) || type[0] == 0) ? __alignof__(void*) : objc_alignof_type(type);
		if (type[0] == '\0')
		{
			n->ivar_list[i].align = size;
		}
		ivarSetOwnership(&n->ivar_list[i], ownershipForIvar(cls, i));
	}
	return n;
}

static int legacy_key;

PRIVATE struct legacy_gnustep_objc_class* objc_legacy_class_for_class(Class cls)
{
	return (struct legacy_gnustep_objc_class*)objc_getAssociatedObject((id)cls, &legacy_key);
}

PRIVATE Class objc_upgrade_class(struct legacy_gnustep_objc_class *oldClass)
{
	Class cls = calloc(sizeof(struct objc_class), 1);
	cls->isa = oldClass->isa;
	cls->super_class = (struct objc_class*)oldClass->super_class;
	cls->name = oldClass->name;
	cls->version = oldClass->version;
	cls->info = oldClass->info;
	cls->instance_size = oldClass->instance_size;
	cls->ivars = upgradeIvarList(oldClass);
	cls->methods = oldClass->methods;
	cls->protocols = oldClass->protocols;
	cls->abi_version = oldClass->abi_version;
	cls->properties = oldClass->properties;
	objc_register_selectors_from_class(cls);
	if (!objc_test_class_flag(cls, objc_class_flag_meta))
	{
		cls->isa = objc_upgrade_class((struct legacy_gnustep_objc_class*)cls->isa);
		objc_setAssociatedObject((id)cls, &legacy_key, (id)oldClass, OBJC_ASSOCIATION_ASSIGN);
	}
	return cls;
}

PRIVATE struct objc_category *objc_upgrade_category(struct objc_category_legacy *old)
{
	struct objc_category *cat = calloc(1, sizeof(struct objc_category));
	memcpy(cat, old, sizeof(struct objc_category_legacy));
	return cat;
}
