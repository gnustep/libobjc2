id objc_autorelease(id obj);
id objc_autoreleaseReturnValue(id obj);
id objc_initWeak(id *object, id value);
id objc_loadWeak(id* object);
id objc_loadWeakRetained(id* obj);
id objc_retain(id obj);
id objc_retainAutorelease(id obj);
id objc_retainAutoreleaseReturnValue(id obj);
id objc_retainAutoreleasedReturnValue(id obj);
id objc_retainBlock(id b);
id objc_storeStrong(id *object, id value);
id objc_storeWeak(id *addr, id obj);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *pool);
void objc_copyWeak(id *dest, id *src);
void objc_destroyWeak(id* obj);
void objc_moveWeak(id *dest, id *src);
void objc_release(id obj);
/**
 * Mark the object as about to begin deallocation.  All subsequent reads of
 * weak pointers will return 0.  This function should be called in -release,
 * before calling [self dealloc].
 *
 * Nonstandard extension.
 */
void objc_delete_weak_refs(id obj);
