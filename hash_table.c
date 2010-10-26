#include "objc/toydispatch.h"


static dispatch_queue_t garbage_queue;

__attribute__((constructor)) void static create_queue(void)
{
	garbage_queue = dispatch_queue_create("ObjC deferred free queue", 0);
}

void objc_collect_garbage_data(void(*cleanup)(void*), void *garbage)
{
	dispatch_async_f(garbage_queue, garbage, cleanup);
}
