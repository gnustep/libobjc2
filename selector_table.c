/**
 * Handle selector uniquing.
 * 
 * When building, you may define TYPE_DEPENDENT_DISPATCH to enable message
 * sends to depend on their types.
 */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "lock.h"
#include "sarray2.h"
#include "objc/runtime.h"
#include "method_list.h"
#include "class.h"
#include "selector.h"

#ifdef TYPE_DEPENDENT_DISPATCH
#	define TDD(x) x
#else
#	define TDD(x)
#endif


// Define the pool allocator for selectors.  This is a simple bump-the-pointer
// allocator for low-overhead allocation.
#define POOL_NAME selector
#define POOL_TYPE struct objc_selector
#include "pool.h"


/**
 * The number of selectors currently registered.  When a selector is
 * registered, its name field is replaced with its index in the selector_list
 * array.  
 */
uint32_t __objc_selector_max_index;
/**
 * Mapping from selector numbers to selector names.
 */
static SparseArray *selector_list  = NULL;

// Get the functions for string hashing
#include "string_hash.h"

inline static BOOL isSelRegistered(SEL sel)
{
	if ((uintptr_t)sel->name < (uintptr_t)__objc_selector_max_index)
	{
		return YES;
	}
	return NO;
}

/**
 * Compare selectors based on whether they are treated as equivalent for the
 * purpose of dispatch.
 */
static int selector_equal(const void *k,
                            const SEL value)
{
	SEL key = (SEL)k;
	return string_compare(sel_getName(key), sel_getName(value)) TDD(&&
		string_compare(sel_getType_np(key), sel_getType_np(value)));
}
/**
 * Compare whether two selectors are identical.
 */
static int selector_identical(const SEL key,
                              const SEL value)
{
	return string_compare(sel_getName(key), sel_getName(value)) &&
		string_compare(sel_getType_np(key), sel_getType_np(value));
}
static inline uint32_t addStringToHash(uint32_t hash, const char *str)
{
	uint32_t c;
	if(str != NULL)
	{
		while((c = (uint32_t)*str++))
		{
			hash = hash * 33 + c;
		}
	}
	return hash;
}
/**
 * Hash a selector.
 */
static inline uint32_t hash_selector(const void *s)
{
	SEL sel = (SEL)s;
	uint32_t hash = 5381;
	hash = addStringToHash(hash, sel_getName(sel));
	hash = addStringToHash(hash, sel->types);
	return hash;
}

#define MAP_TABLE_NAME selector 
#define MAP_TABLE_COMPARE_FUNCTION selector_equal
#define MAP_TABLE_HASH_KEY hash_selector
#define MAP_TABLE_HASH_VALUE hash_selector
#include "hash_table.h"
/**
 * Table of registered selector.  Maps from selector to selector.
 */
static selector_table *sel_table;

/**
 * Lock protecting the selector table.
 */
mutex_t selector_table_lock;


/**
 * Hack to make the uninstalled dtable the right size.  Won't be needed with sarray2.
 */
void objc_resize_uninstalled_dtable(void);

/**
 * Create data structures to store selectors.
 */
void __objc_init_selector_tables()
{
	selector_list = SparseArrayNew();
	INIT_LOCK(selector_table_lock);
	sel_table = selector_create(40960);
}

static SEL selector_lookup(const char *name, const char *types)
{
	struct objc_selector sel = {name, types};
	return selector_table_get(sel_table, &sel);
}
static inline void add_selector_to_table(SEL aSel, int32_t uid, uint32_t idx)
{
	struct sel_type_list *typeList =
		(struct sel_type_list *)selector_pool_alloc();
	typeList->value = aSel->name;
	typeList->next = 0;
	// Store the name.
	SparseArrayInsert(selector_list, idx, typeList);
	// Store the selector.
	selector_insert(sel_table, aSel);
	// Set the selector's name to the uid.
	aSel->name = (const char*)uid;
}
/**
 * Really registers a selector.  Must be called with the selector table locked.
 */
static inline void register_selector_locked(SEL aSel)
{
	uintptr_t idx = __objc_selector_max_index++;
	if (NULL == aSel->types)
	{
		add_selector_to_table(aSel, idx, idx);
		objc_resize_uninstalled_dtable();
		return;
	}
	SEL untyped = selector_lookup(aSel->name, 0);
	// If this has a type encoding, store the untyped version too.
	if (untyped == NULL)
	{
		untyped = selector_pool_alloc();
		untyped->name = aSel->name;
		untyped->types = 0;
		add_selector_to_table(untyped, idx, idx);
		// If we are in type dependent dispatch mode, the uid for the typed
		// and untyped versions will be different
		idx++; __objc_selector_max_index++;
	}
	uintptr_t uid = (uintptr_t)untyped->name;
	TDD(uid = idx);
	add_selector_to_table(aSel, uid, idx);

	// Add this set of types to the list.
	// This is quite horrible.  Most selectors will only have one type
	// encoding, so we're wasting a lot of memory like this.
	struct sel_type_list *typeListHead =
		SparseArrayLookup(selector_list, (uint32_t)(uintptr_t)untyped->name);
	struct sel_type_list *typeList =
		(struct sel_type_list *)selector_pool_alloc();
	typeList->value = aSel->types;
	typeList->next = typeListHead->next;
	typeListHead->next = typeList;
	objc_resize_uninstalled_dtable();
}
/**
 * Registers a selector.  This assumes that the argument is never deallocated.
 */
static SEL objc_register_selector(SEL aSel)
{
	if (isSelRegistered(aSel))
	{
		return aSel;
	}
	// Check that this isn't already registered, before we try 
	SEL registered = selector_lookup(aSel->name, aSel->types);
	if (NULL != registered && (selector_identical(aSel, registered) || NULL == aSel->types))
	{
		aSel->name = registered->name;
		return registered;
	}
	LOCK(&selector_table_lock);
	register_selector_locked(aSel);
	UNLOCK(&selector_table_lock);
	return aSel;
}

/**
 * Registers a selector by copying the argument.  
 */
static SEL objc_register_selector_copy(SEL aSel)
{
	// If an identical selector is already registered, return it.
	SEL copy = selector_lookup(aSel->name, aSel->types);
	if (NULL != copy && (selector_identical(aSel, copy) || NULL == aSel->types))
	{
		return copy;
	}
	LOCK(&selector_table_lock);
	// Create a copy of this selector.
	copy = selector_pool_alloc();
	copy->name = strdup(aSel->name);
	copy->types = (NULL == aSel->types) ? NULL : strdup(aSel->types);
	// Try to register the copy as the authoritative version
	register_selector_locked(copy);
	UNLOCK(&selector_table_lock);
	return copy;
}

/**
 * Public API functions.
 */

const char *sel_getName(SEL sel)
{
	const char *name = sel->name;
	if (isSelRegistered(sel))
	{
		struct sel_type_list * list =
			SparseArrayLookup(selector_list, (uint32_t)(uintptr_t)sel->name);
		name = (list == NULL) ? NULL : list->value;
	}
	if (NULL == name)
	{
		name = "";
	}
	return name;
}

SEL sel_getUid(const char *selName)
{
	return selector_lookup(selName, 0);
}

BOOL sel_isEqual(SEL sel1, SEL sel2)
{
	return selector_equal(sel1, sel2);
}

SEL sel_registerName(const char *selName)
{
	struct objc_selector sel = {selName, 0};
	return objc_register_selector_copy(&sel);
}

SEL sel_registerTypedName_np(const char *selName, const char *types)
{
	struct objc_selector sel = {selName, types};
	return objc_register_selector_copy(&sel);
}

const char *sel_getType_np(SEL aSel)
{
	return (NULL == aSel->types) ? "" : aSel->types;
}


unsigned sel_copyTypes(const char *selName, const char **types, unsigned count)
{
	SEL untyped = selector_lookup(selName, 0);
	if (untyped == NULL) { return 0; }

	struct sel_type_list *l =
		SparseArrayLookup(selector_list, (uint32_t)(uintptr_t)untyped->name);
	// Skip the head, which just contains the name, not the types.
	l = l->next;

	if (count == 0)
	{
		while (NULL != l)
		{
			count++;
			l = l->next;
		}
		return count;
	}

	unsigned found = 0;
	while (NULL != l && found<count)
	{
		types[found++] = l->value;
		l = l->next;
	}
	return found;
}

void __objc_register_selectors_from_list(struct objc_method_list *l)
{
	for (int i=0 ; i<l->count ; i++)
	{
		Method m = &l->methods[i];
		struct objc_selector sel = { (const char*)m->selector, m->types };
		m->selector = objc_register_selector_copy(&sel);
	}
}
/**
 * Register all of the (unregistered) selectors that are used in a class.
 */
void __objc_register_selectors_from_class(Class class)
{
	for (struct objc_method_list *l=class->methods ; NULL!=l ; l=l->next)
	{
		__objc_register_selectors_from_list(l);
	}
}
void __objc_register_selector_array(SEL selectors, unsigned long count)
{
	for (unsigned long i=0 ; (i<count) && (NULL != selectors[i].name) ; i++)
	{
		objc_register_selector(&selectors[i]);
	}
}

// FIXME: This is a stupid idea.  We should be inserting root class instance
// methods into the dtable, not duplicating the metadata stuff.  Yuck!
void __objc_register_instance_methods_to_class (Class class);

/**
 * Legacy GNU runtime compatibility.
 *
 * All of the functions in this section are deprecated and should not be used
 * in new code.
 */

SEL sel_get_typed_uid (const char *name, const char *types)
{
	SEL sel = selector_lookup(name, types);
	struct sel_type_list *l =
		SparseArrayLookup(selector_list, (uint32_t)(uintptr_t)sel->name);
	// Skip the head, which just contains the name, not the types.
	l = l->next;
	if (NULL != l)
	{
		sel = selector_lookup(name, l->value);
	}
	return sel;
}

SEL sel_get_any_typed_uid (const char *name)
{
	return selector_lookup(name, 0);
}

SEL sel_get_any_uid (const char *name)
{
	return selector_lookup(name, 0);
}

SEL sel_get_uid(const char *name)
{
	return selector_lookup(name, 0);
}

const char *sel_get_name(SEL selector)
{
	return sel_getName(selector);
}

BOOL sel_is_mapped(SEL selector)
{
	return isSelRegistered(selector);
}

const char *sel_get_type(SEL selector)
{
	return sel_getType_np(selector);
}

SEL sel_register_name(const char *name)
{
	return sel_registerName(name);
}

SEL sel_register_typed_name (const char *name, const char *type)
{
	return sel_registerTypedName_np(name, type);
}

/*
 * Some simple sanity tests.
 */
#ifdef SEL_TEST
static void logSelector(SEL sel)                         
{                                                        
	fprintf(stderr, "%s = {%p, %s}\n", sel_getName(sel), sel->name, sel_getType_np(sel));
}
void objc_resize_uninstalled_dtable(void) {}

int main(void)
{
	__objc_init_selector_tables();
	SEL a = sel_registerTypedName_np("foo:", "1234");
	logSelector(a);
	a = sel_registerName("foo:");
	logSelector(a);
	logSelector(sel_get_any_typed_uid("foo:"));
	a = sel_registerTypedName_np("foo:", "1234");
	logSelector(a);
	logSelector(sel_get_any_typed_uid("foo:"));
	a = sel_registerTypedName_np("foo:", "456");
	logSelector(a);
	unsigned count = sel_copyTypes("foo:", NULL, 0);
	const char *types[count];
	sel_copyTypes("foo:", types, count);
	for (unsigned i=0 ; i<count ; i++)
	{
		fprintf(stderr, "Found type %s\n", types[i]);
	}
	fprintf(stderr, "Number of types: %d\n", count);
	SEL sel;
}
#endif
