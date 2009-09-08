#include <objc/hash.h>
#include <objc/objc-api.h>

static cache_ptr Protocols = 0;

struct objc_method_description_list 
{
	int count;
	struct objc_method_description list[1];
};

static Class ObjC2ProtocolClass = 0;

void __objc_init_protocol_table(void)
{
	Protocols = objc_hash_new(128,
	                          (hash_func_type)objc_hash_string,
	                          (compare_func_type)objc_compare_strings);
}  


static int isEmptyProtocol(struct objc_protocol *aProto)
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
static void makeProtocolEqualToProtocol(struct objc_protocol *p1,
                                        struct objc_protocol *p2) {}

void __objc_unique_protocol(struct objc_protocol *aProto)
{
	if (ObjC2ProtocolClass == 0)
	{
		ObjC2ProtocolClass = objc_get_class("Protocol2");
	}
	struct objc_protocol *oldProtocol = 
		objc_hash_value_for_key(Protocols, aProto->protocol_name);
	if (NULL == oldProtocol)
	{
		// This is the first time we've seen this protocol, so add it to the
		// hash table and ignore it.
		objc_hash_add(&Protocols, aProto, aProto->protocol_name);
		return;
	}
	if (isEmptyProtocol(oldProtocol))
	{
		if (isEmptyProtocol(aProto))
		{
			// Add protocol to a list somehow.
		}
		else
		{
			// This protocol is not empty, so we use its definitions
			makeProtocolEqualToProtocol(oldProtocol, aProto);
		}
	}
	else
	{
		if (isEmptyProtocol(aProto))
		{
			makeProtocolEqualToProtocol(aProto, oldProtocol);
		}
		else
		{
			//FIXME: We should really perform a check here to make sure the
			//protocols are actually the same.
		}
	}
}
