#include "objc/toydispatch.h"
#include "lock.h"


static dispatch_queue_t garbage_queue;

void objc_collect_garbage_data(void(*cleanup)(void*), void *garbage)
{
	if (0 == garbage_queue)
	{
		LOCK(__objc_runtime_mutex);
		if (0 == garbage_queue)
		{
			garbage_queue = dispatch_queue_create("ObjC deferred free queue", 0);
		}
		UNLOCK(__objc_runtime_mutex);
	}
	dispatch_async_f(garbage_queue, garbage, cleanup);
}
