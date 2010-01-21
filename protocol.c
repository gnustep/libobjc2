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
// This defines the maximum number of classes that the runtime supports.
#define MAP_TABLE_STATIC_SIZE 2048
#include "hash_table.h"

static protocol_table known_protocol_table;

static mutex_t protocol_table_lock;

void __objc_init_protocol_table(void)
{
	LOCK(__objc_runtime_mutex);
	INIT_LOCK(protocol_table_lock);
	UNLOCK(__objc_runtime_mutex);
}  

static void protocol_table_insert(const struct objc_protocol2 *protocol)
{
	LOCK(&protocol_table_lock);
	protocol_insert(&known_protocol_table, protocol->protocol_name, (void*)protocol);
	UNLOCK(&protocol_table_lock);
}

struct objc_protocol2 *protocol_for_name(const char *protocol_name)
{
	return protocol_table_get(&known_protocol_table, protocol_name);
}

struct objc_method_description_list
{
	int count;
	struct objc_method_description list[1];
};

static Class ObjC2ProtocolClass = 0;

static int isEmptyProtocol(struct objc_protocol2 *aProto)
{
	int isEmpty = (aProto->instance_methods->count == 0) &&
		(aProto->class_methods->count == 0) &&
		(aProto->protocol_list->count == 0);
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
