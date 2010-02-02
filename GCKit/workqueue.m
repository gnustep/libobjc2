#include "../toydispatch/toydispatch.h"
#include <unistd.h>
#include <stdlib.h>

static dispatch_queue_t gc_deferred_queue;
static dispatch_queue_t gc_queue;

__attribute__((constructor)) void static create_queue(void)
{
	gc_deferred_queue = dispatch_queue_create("GCKit collection deferred queue", 0);
	gc_queue = dispatch_queue_create("GCKit collection queue", 0);
}

struct gc_deferred_execution_t
{
	useconds_t time;
	dispatch_function_t function;
	void *data;
};

static void perform_deferred(void *c)
{
	struct gc_deferred_execution_t *context = c;
	usleep(context->time);
	dispatch_async_f(gc_queue, context->function, context->data);
	free(context);
}

/**
 * Runs function(data) at some point at least useconds microseconds in the
 * future.
 */
void GCPerformDeferred(dispatch_function_t function, void *data, 
		int useconds)
{
	struct gc_deferred_execution_t *context = malloc(sizeof(struct gc_deferred_execution_t));
	context->time = useconds;
	context->function = function;
	context->data = data;
	dispatch_async_f(gc_deferred_queue, context, perform_deferred);
}

void GCPerform(dispatch_function_t function, void *data)
{
	dispatch_async_f(gc_queue, data, function);
}
