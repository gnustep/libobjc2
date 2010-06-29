#ifndef OBJC_SELECTOR_H_INCLUDED
#define OBJC_SELECTOR_H_INCLUDED
/**
 * Structure used to store the types for a selector.  This allows for a quick
 * test to see whether a selector is polymorphic and allows enumeration of all
 * type encodings for a given selector.
 *
 * This is the same size as an objc_selector, so we can allocate them from the
 * objc_selector pool.
 *
 * Note: For ABI v10, we can probably do something a bit more sensible here and
 * make selectors into a linked list.
 */
struct sel_type_list
{
	const char *value;
	struct sel_type_list *next;
};

/**
 * Structure used to store selectors in the list.
 */
struct objc_selector
{
	const char * name;
	const char * types;
};

__attribute__((unused))
static uint32_t get_untyped_idx(SEL aSel)
{
	SEL untyped = sel_registerTypedName_np(sel_getName(aSel), 0);
	return (uint32_t)(uintptr_t)untyped->name;
}


#endif // OBJC_SELECTOR_H_INCLUDED
