#include "objc/objc.h"
#include "objc/objc-api.h"
#include "lock.h"
#include <stdlib.h>

// Get the functions for string hashing
#include "string_hash.h"

static int protocol_compare(const char *name, 
                            const struct objc_protocol2 *protocol)
{
	return string_compare(name, protocol->protocol_name);
}
static int protocol_hash(const struct objc_protocol2 *protocol)
{
	return string_hash(protocol->protocol_name);
}
#define MAP_TABLE_NAME protocol
#define MAP_TABLE_COMPARE_FUNCTION protocol_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE protocol_hash
#include "hash_table.h"

static protocol_table *known_protocol_table;

void __objc_init_protocol_table(void)
{
	known_protocol_table = protocol_create(128);
}  

static void protocol_table_insert(const struct objc_protocol2 *protocol)
{
	protocol_insert(known_protocol_table, (void*)protocol);
}

struct objc_protocol2 *protocol_for_name(const char *protocol_name)
{
	return protocol_table_get(known_protocol_table, protocol_name);
}

struct objc_method_description_list
{
	int count;
	struct objc_method_description list[1];
};

static Class ObjC2ProtocolClass = 0;

static int isEmptyProtocol(struct objc_protocol2 *aProto)
{
	int isEmpty = 
		((aProto->instance_methods == NULL) || 
			(aProto->instance_methods->count == 0)) &&
		((aProto->class_methods == NULL) || 
			(aProto->class_methods->count == 0)) &&
		((aProto->protocol_list == NULL) ||
		  (aProto->protocol_list->count == 0));
	if (aProto->class_pointer == ObjC2ProtocolClass)
	{
		struct objc_protocol2 *p2 = (struct objc_protocol2*)aProto;
		isEmpty &= (p2->optional_instance_methods->count == 0);
		isEmpty &= (p2->optional_class_methods->count == 0);
		isEmpty &= (p2->properties->property_count == 0);
		isEmpty &= (p2->optional_properties->property_count == 0);
	}
	return isEmpty;
}

// FIXME: Make p1 adopt all of the stuff in p2
static void makeProtocolEqualToProtocol(struct objc_protocol2 *p1,
                                        struct objc_protocol2 *p2) 
{
#define COPY(x) p1->x = p2->x
	COPY(instance_methods);
	COPY(class_methods);
	COPY(protocol_list);
	if (p1->class_pointer == ObjC2ProtocolClass &&
		p2->class_pointer == ObjC2ProtocolClass)
	{
		COPY(optional_instance_methods);
		COPY(optional_class_methods);
		COPY(properties);
		COPY(optional_properties);
	}
#undef COPY
}

struct objc_protocol2 *__objc_unique_protocol(struct objc_protocol2 *aProto)
{
	if (ObjC2ProtocolClass == 0)
	{
		ObjC2ProtocolClass = objc_get_class("Protocol2");
	}
	struct objc_protocol2 *oldProtocol = 
		protocol_for_name(aProto->protocol_name);
	if (NULL == oldProtocol)
	{
		// This is the first time we've seen this protocol, so add it to the
		// hash table and ignore it.
		protocol_table_insert(aProto);
		return aProto;
	}
	if (isEmptyProtocol(oldProtocol))
	{
		if (isEmptyProtocol(aProto))
		{
			return aProto;
			// Add protocol to a list somehow.
		}
		else
		{
			// This protocol is not empty, so we use its definitions
			makeProtocolEqualToProtocol(oldProtocol, aProto);
			return aProto;
		}
	}
	else
	{
		if (isEmptyProtocol(aProto))
		{
			makeProtocolEqualToProtocol(aProto, oldProtocol);
			return oldProtocol;
		}
		else
		{
			return oldProtocol;
			//FIXME: We should really perform a check here to make sure the
			//protocols are actually the same.
		}
	}
}


// Public functions:
Protocol *objc_getProtocol(const char *name)
{
	return (Protocol*)protocol_for_name(name);
}

BOOL protocol_conformsToProtocol(Protocol *p, Protocol *other)
{

	return NO;
}

struct objc_method_description *protocol_copyMethodDescriptionList(Protocol *p,
	BOOL isRequiredMethod, BOOL isInstanceMethod, unsigned int *count)
{
	*count = 0;
	return NULL;
}

Protocol **protocol_copyProtocolList(Protocol *p, unsigned int *count)
{
	*count = 0;
	return NULL;
}

const char *protocol_getName(Protocol *p)
{
	if (NULL != p)
	{
		return p->protocol_name;
	}
	return NULL;
}

BOOL protocol_isEqual(Protocol *p, Protocol *other)
{
	if (NULL == p || NULL == other)
	{
		return NO;
	}
	if (p == other || 
		p->protocol_name == other->protocol_name ||
		0 == strcmp(p->protocol_name, other->protocol_name))
	{
		return YES;
	}
	return NO;
}

