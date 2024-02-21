#include "objc/runtime.h"
#include "class.h"

typedef struct _NSZone NSZone;
@interface RootMethods
- (id)alloc;
- (id)allocWithZone: (NSZone*)aZone;
- (id)init;
@end
#include <stdio.h>

/**
 * Equivalent to [cls alloc].  If there's a fast path opt-in, then this skips the message send.
 */
OBJC_PUBLIC
id
objc_alloc(Class cls)
{
	if (UNLIKELY(!objc_test_class_flag(cls->isa, objc_class_flag_initialized)))
	{
		objc_send_initialize(cls);
	}
	if (objc_test_class_flag(cls->isa, objc_class_flag_fast_alloc_init))
	{
		return class_createInstance(cls, 0);
	}
    return [cls alloc];
}

/**
 * Equivalent to [cls allocWithZone: null].  If there's a fast path opt-in, then this skips the message send.
 */
OBJC_PUBLIC
id
objc_allocWithZone(Class cls)
{
	if (UNLIKELY(!objc_test_class_flag(cls->isa, objc_class_flag_initialized)))
	{
		objc_send_initialize(cls);
	}
	if (objc_test_class_flag(cls->isa, objc_class_flag_fast_alloc_init))
	{
		return class_createInstance(cls, 0);
	}
	return [cls allocWithZone: NULL];
}

/**
 * Equivalent to [[cls alloc] init].  If there's a fast path opt-in, then this
 * skips the message send.
 */
OBJC_PUBLIC
id
objc_alloc_init(Class cls)
{
	id instance = objc_alloc(cls);
	if (objc_test_class_flag(cls, objc_class_flag_fast_alloc_init))
	{
		return instance;
	}
	return [instance init];
}
