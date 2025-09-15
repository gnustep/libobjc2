#ifndef __OBJC_LOADER_H_INCLUDED
#define __OBJC_LOADER_H_INCLUDED
#include "category.h"
#include "method.h"
#include "module.h"
#include "class.h"
#include "protocol.h"

/**
 * Checks whether it is safe to load a module with the specified version and
 * module size.  This depends on whether another module with an incompatible
 * ABI has already been loaded.
 */
BOOL objc_check_abi_version(struct objc_module_abi_8 *module);
/**
 * Initializes a protocol list, uniquing the protocols in the list.
 */
void objc_init_protocols(struct objc_protocol_list *protocols);
/**
 * Registers a set of selectors from a method list.
 */
void objc_register_selectors_from_list(struct objc_method_list *l);
/**
 * Register all of the (unregistered) selectors that are used in a class.
 */
void objc_register_selectors_from_class(Class class);
/**
 * Registers all of the selectors in an array.
 */
void objc_register_selector_array(SEL selectors, unsigned long count);
/**
 * Loads a class into the runtime system.  If possible, the class is resolved
 * (inserted into the class tree) immediately.  If its superclass is not yet
 * resolved, it is enqueued for later resolution.
 */
void objc_load_class(struct objc_class *class);
/**
 * Resolves classes that have not yet been resolved, if their superclasses have
 * subsequently been loaded.
 */
void objc_resolve_class_links(void);
/**
 * Attaches a category to its class, if the class is already loaded.  Buffers
 * it for future resolution if not.
 */
void objc_try_load_category(struct objc_category *cat);
/**
 * Tries to load all of the categories that could not previously be loaded
 * because their classes were not yet loaded.
 */
void objc_load_buffered_categories(void);
/**
 * Updates the dispatch table for a class.  
 */
void objc_update_dtable_for_class(Class cls);
/**
 * Initialises a list of static object instances belonging to the same class if
 * possible, or defers initialisation until the class has been loaded it not.
 */
void objc_init_statics(struct objc_static_instance_list *statics);
/**
 * Tries again to initialise static instances which could not be initialised
 * earlier.
 */
void objc_init_buffered_statics(void);

/**
 * Initialise built-in classes (Object and Protocol).  This must be called
 * after `init_class_tables`.
 */
void init_builtin_classes(void);

/**
 * Initialise the aliases table.
 */
void init_alias_table(void);

/**
 * Initialise the automatic reference counting system.
 */
void init_arc(void);

/**
 * Initialise the class tables.
 */
void init_class_tables(void);

/**
 * Initialise the dispatch table machinery.
 */
void init_dispatch_tables(void);

/**
 * Initialise the protocol tables.
 */
void init_protocol_table(void);

/**
 * Initialise the selector tables.
 */
void init_selector_tables(void);

/**
 * Initialise the trampolines for using blocks as methods.
 */
void init_trampolines(void);

/**
 * Send +load messages to a class if required.
 */
void objc_send_load_message(Class cls);

/**
 * Resolve a class (populate its superclass and sibling class links).  Returns
 * YES if the class can be resolved, NO otherwise.  Classes cannot be resolved
 * unless their superclasses have all been resolved.
 */
BOOL objc_resolve_class(Class cls);

/**
 * Initialise the block classes.
 */
void init_early_blocks(void);


#endif //__OBJC_LOADER_H_INCLUDED
