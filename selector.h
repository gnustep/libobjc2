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

#endif // OBJC_SELECTOR_H_INCLUDED
