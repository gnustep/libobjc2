#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "objc/runtime.h"
#include "nsobject.h"
#include "spinlock.h"
#include "class.h"
#include "dtable.h"
#include "selector.h"

/**
 * A single associative reference.  Contains the key, value, and association
 * policy.
 */
struct reference
{
	/**
	 * The key used for identifying this object.  Opaque pointer, should be set
	 * to 0 when this slot is unused.
	 */
	void *key;
	/**
	 * The associated object.  Note, if the policy is assign then this may be
	 * some other type of pointer...
	 */
	void *object;
	/**
	 * Association policy.
	 */
	uintptr_t policy;
};

#define REFERENCE_LIST_SIZE 10

/**
 * Linked list of references associated with an object.  We assume that there
 * won't be very many, so we don't bother with a proper hash table, and just
 * iterate over a list.
 */
struct reference_list
{
	/**
	 * Next group of references.  This is only ever used if we have more than
	 * 10 references associated with an object, which seems highly unlikely.
	 */
	struct reference_list *next;
	/**
	 * Array of references.
	 */
	struct reference list[REFERENCE_LIST_SIZE];
};
enum
{
	OBJC_ASSOCIATION_ATOMIC = 0x300,
};

static BOOL isAtomic(uintptr_t policy)
{
	return (policy & OBJC_ASSOCIATION_ATOMIC) == OBJC_ASSOCIATION_ATOMIC;
}

static struct reference* findReference(struct reference_list *list, void *key)
{
	if (NULL == list) { return NULL; }

	for (int i=0 ; i<REFERENCE_LIST_SIZE ; i++)
	{
		if (list->list[i].key == key)
		{
			return &list->list[i];
		}
	}
	return NULL;
}
static void cleanupReferenceList(struct reference_list *list)
{
	if (NULL == list) { return; }

	for (int i=0 ; i<REFERENCE_LIST_SIZE ; i++)
	{
		struct reference *r = &list->list[i];
		if (0 != r->key)
		{
			r->key = 0;
			if (OBJC_ASSOCIATION_ASSIGN != r->policy)
			{
				// Full barrier - ensure that we've zero'd the key before doing
				// this!
				__sync_synchronize();
				[(id)r->object release];
			}
			r->object = 0;
			r->policy = 0;
		}
	}
}

static void setReference(struct reference_list *list,
                         void *key,
                         void *obj,
                         uintptr_t policy)
{
	switch (policy)
	{
		// Ignore any unknown association policies
		default: return;
		case OBJC_ASSOCIATION_COPY_NONATOMIC:
		case OBJC_ASSOCIATION_COPY:
			obj = [(id)obj copy];
			break;
		case OBJC_ASSOCIATION_RETAIN_NONATOMIC:
		case OBJC_ASSOCIATION_RETAIN:
			obj = [(id)obj retain];
		case OBJC_ASSOCIATION_ASSIGN:
			break;
	}
	// While inserting into the list, we need to lock it temporarily.
	int *lock = lock_for_pointer(list);
	lock_spinlock(lock);
	struct reference *r = findReference(list, key);
	// If there's an existing reference, then we can update it, otherwise we
	// have to install a new one
	if (NULL == r)
	{
		// Search for an unused slot
		r = findReference(list, 0);
		if (NULL == r)
		{
			struct reference_list *l = list;

			while (NULL != l->next) { l = l->next; }

			l->next = calloc(1, sizeof(struct reference_list));
			r = &l->next->list[0];
		}
		r->key = key;
	}
	unlock_spinlock(lock);
	// Now we only need to lock if the old or new property is atomic
	BOOL needLock = isAtomic(r->policy) || isAtomic(policy);
	if (needLock)
	{
		lock = lock_for_pointer(r);
		lock_spinlock(lock);
	}
	r->policy = policy;
	id old = r->object;
	r->object = obj;
	if (OBJC_ASSOCIATION_ASSIGN != r->policy)
	{
		[old release];
	}
	if (needLock)
	{
		unlock_spinlock(lock);
	}
}

static void deallocHiddenClass(id obj, SEL _cmd);

static inline Class findHiddenClass(id obj)
{
	Class cls = obj->isa;
	while (Nil != cls && 
	       !objc_test_class_flag(cls, objc_class_flag_assoc_class))
	{
		cls = class_getSuperclass(cls);
	}
	return cls;
}

static Class allocateHiddenClass(Class superclass)
{
	Class newClass =
		calloc(1, sizeof(struct objc_class) + sizeof(struct reference_list));

	if (Nil == newClass) { return Nil; }

	// Set up the new class
	newClass->isa = superclass->isa;
	// Set the superclass pointer to the name.  The runtime will fix this when
	// the class links are resolved.
	newClass->name = superclass->name;
	newClass->info = objc_class_flag_resolved | 
		objc_class_flag_class | objc_class_flag_user_created |
		objc_class_flag_new_abi | objc_class_flag_hidden_class |
		objc_class_flag_assoc_class;
	newClass->super_class = superclass;
	newClass->dtable = uninstalled_dtable;
	newClass->instance_size = superclass->instance_size;
	if (objc_test_class_flag(superclass, objc_class_flag_meta))
	{
		newClass->info |= objc_class_flag_meta;
	}

	return newClass;
}

static inline Class initHiddenClassForObject(id obj)
{
	Class hiddenClass = allocateHiddenClass(obj->isa); 
	if (class_isMetaClass(obj->isa))
	{
		obj->isa = hiddenClass;
	}
	else
	{
		const char *types =
			method_getTypeEncoding(class_getInstanceMethod(obj->isa,
						SELECTOR(dealloc)));
		class_addMethod(hiddenClass, SELECTOR(dealloc), (IMP)deallocHiddenClass,
				types);
		obj->isa = hiddenClass;
	}
	return hiddenClass;
}

static void deallocHiddenClass(id obj, SEL _cmd)
{
	Class hiddenClass = findHiddenClass(obj);
	Class realClass = class_getSuperclass(hiddenClass);
	// Call the real -dealloc method (this ordering is required in case the
	// user does @synchronized(self) in -dealloc)
	struct objc_super super = {obj, realClass};
	objc_msg_lookup_super(&super, SELECTOR(dealloc))(obj, SELECTOR(dealloc));
	// After calling [super dealloc], the object will no longer exist.
	// Free the hidden
	struct reference_list *list = object_getIndexedIvars(hiddenClass);
	cleanupReferenceList(list);

	// FIXME: Low memory profile.
	SparseArrayDestroy(hiddenClass->dtable);

	// Free the class
	free(hiddenClass);
}

static struct reference_list* referenceListForObject(id object, BOOL create)
{
	if (class_isMetaClass(object->isa))
	{
		Class cls = (Class)object;
		if ((NULL == cls->extra_data) && create)
		{
			int *lock = lock_for_pointer(cls);
			lock_spinlock(lock);
			if (NULL == cls->extra_data)
			{
				cls->extra_data = calloc(1, sizeof(struct reference_list));
			}
			unlock_spinlock(lock);
		}
		return cls->extra_data;
	}
	Class hiddenClass = findHiddenClass(object);
	if ((NULL == hiddenClass) && create)
	{
		int *lock = lock_for_pointer(object);
		lock_spinlock(lock);
		hiddenClass = findHiddenClass(object);
		if (NULL == hiddenClass)
		{
			hiddenClass = initHiddenClassForObject(object);
		}
		unlock_spinlock(lock);
	}
	return hiddenClass ? object_getIndexedIvars(hiddenClass) : NULL;
}

void objc_setAssociatedObject(id object,
                              void *key,
                              id value,
                              objc_AssociationPolicy policy)
{
	struct reference_list *list = referenceListForObject(object, YES);
	setReference(list, key, value, policy);
}

id objc_getAssociatedObject(id object, void *key)
{
	struct reference_list *list = referenceListForObject(object, NO);
	if (NULL == list) { return nil; }
	struct reference *r = findReference(list, key);
	return r ? r->object : nil;
}


void objc_removeAssociatedObjects(id object)
{
	cleanupReferenceList(referenceListForObject(object, NO));
}
