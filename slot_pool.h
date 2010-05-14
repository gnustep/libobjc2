#define POOL_NAME slot
#define POOL_TYPE struct objc_slot
#include "pool.h"

static inline struct objc_slot *new_slot_for_method_in_class(Method *method, 
                                                             Class class)
{
	struct objc_slot *slot = slot_pool_alloc();
	slot->owner = class;
	slot->types = method->method_types;
	slot->method = method->method_imp;
	slot->version = 1;
	return slot;
}
