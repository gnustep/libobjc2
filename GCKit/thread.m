#include "../objc/runtime.h"
	BOOL forceCollect;
#import "object.h"
#import "thread.h"
#import "cycle.h"
#import "trace.h"
#import "workqueue.h"

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <sys/mman.h>
#include <unistd.h>
/**
 * Size of the buffers used in each thread before passing stuff over to the GC
 * thread.  Once either BUFFER_SIZE objects are queued waiting for tracing or
 * cycle detection, the queue is passed over to the GC thread, which wakes up
 * and tries to find some things to delete.
 */
static const int BUFFER_SIZE = 256;


/**
 * Thread local storage key used for the thread structure.
 */
static pthread_key_t gc_thread_key;
/**
 *
 */
static pthread_mutex_t thread_lock;

static void GCDrainThread(GCThread *thread, BOOL forceCollect);

GCThread *GCThreadList;

/**
 * Removes this thread from the list.  Must run in the GC thread.
 */
static void GCUnregisterThread(void *t)
{
	GCThread *thr = t;
	if (NULL == thr->last)
	{
		GCThreadList = thr->next;
	}
	else
	{
		thr->last->next = thr->next;
	}
	if (NULL == thr->next)
	{
		if (thr->last)
		{
			thr->next->last = thr->last;
		}
		else
		{
			thr->next->last = GCThreadList;
		}
	}
	//FIXME: Delete tracer references to this stack
	// Wake up the caller thread and let it do the real cleanup
	pthread_mutex_lock(&thread_lock);
	pthread_cond_signal(thr->exitCondition);
}

/**
 * Adds the thread to the list.  Must run in the GC thread.
 */
static void GCAddThread(void *t)
{
	GCThread *thr = t;
	thr->next = GCThreadList;
	if (GCThreadList)
	{
		GCThreadList->last = thr;
	}
	GCThreadList = thr;
}

/**
 * Cleanup function called when a thread is destroyed.  Pops all autorelease
 * pools, deletes all unaliased objects, and so on.
 */
static void cleanup_thread(void *thread)
{
	GCThread *thr = thread;
	GCDrainThread(thread, NO);
	pthread_setspecific(gc_thread_key, NULL);
	pthread_cond_t thread_exit_condition;
	pthread_cond_init(&thread_exit_condition, NULL);
	thr->exitCondition = &thread_exit_condition;
	pthread_mutex_lock(&thread_lock);
	GCPerform(GCUnregisterThread, thread);
	pthread_cond_wait(&thread_exit_condition, &thread_lock);
	pthread_cond_destroy(&thread_exit_condition);
	free(thr->cycleBuffer);
	free(thr->freeBuffer);
	free(thr);
}

/**
 * Thread system initialization.
 */
__attribute__((constructor))
static void init_thread_system(void)
{
	pthread_key_create(&gc_thread_key, cleanup_thread);
	pthread_mutex_init(&thread_lock, NULL);
	GCRegisterThread();
}


void GCRegisterThread(void)
{
	assert(NULL == pthread_getspecific(gc_thread_key) && 
			"Only one thread per thread!");
	GCThread *thr = calloc(sizeof(GCThread),1);
	// Store this in TLS
	pthread_setspecific(gc_thread_key, thr);
	// FIXME: Use non-portable pthread calls to find the stack.
	// This code is, basically, completely wrong.
	char a;
	thr->stackTop = &a;
	while ((intptr_t)thr->stackTop % 4096)
	{ 
		thr->stackTop = ((char*)thr->stackTop)+1;
	}

	thr->cycleBuffer = calloc(BUFFER_SIZE, sizeof(void*));
	thr->freeBuffer = calloc(BUFFER_SIZE, sizeof(void*));
	GCPerform(GCAddThread, thr);
}
void GCAddObject(id anObject)
{
	GCThread *thr = pthread_getspecific(gc_thread_key);
	// If the reference count is 0, we add this 
	if (GCGetRetainCount(anObject) == 0)
	{
		thr->freeBuffer[thr->freeBufferInsert++] = anObject;
		if (thr->freeBufferInsert == BUFFER_SIZE)
		{
			GCDrainThread(thr, NO);
		}
	}
	else if (!GCTestFlag(anObject, GCFlagBuffered))
	{
		// Note: there is a potential race here.  If this occurs then two
		// GCAutoreleasePools might add the same object to their buffers.  This
		// is not important.  If it does happen then we run the cycle detector
		// on an object twice.  This increases the complexity of the collector
		// above linear, but the cost of making sure that it never happens is
		// much greater than the cost of (very) occasionally checking an object
		// twice if it happens to be added at exactly the same time by two
		// threads.
		GCSetFlag(anObject, GCFlagBuffered);
		thr->cycleBuffer[thr->cycleBufferInsert++] = anObject;
		if (thr->cycleBufferInsert == BUFFER_SIZE)
		{
			GCDrainThread(thr, NO);
		}
	}
}

typedef struct 
{
	id *cycleBuffer;
	unsigned int cycleBufferSize;
	BOOL forceTrace;
} GCTraceContext;

/**
 * Trampoline that is run in the GC thread.
 */
static void traceTrampoline(void *c)
{
	GCTraceContext *context = c;

	// Scan for any new garbage cycles.
	if (context->cycleBufferSize)
	{
		GCScanForCycles(context->cycleBuffer, context->cycleBufferSize);
		free(context->cycleBuffer);
	}
	// Now add the objects that might be garbage to the collector.
	// These won't actually be freed until after this 
	GCRunTracerIfNeeded(context->forceTrace);

	//free(c);
}

/**
 * Collect garbage cycles.
 */
static void GCDrainThread(GCThread *thread, BOOL forceCollect)
{
	// Register these objects for tracing
	GCAddObjectsForTracing(thread);
	// Tweak the bottom of the stack to be in this stack frame.  Anything in
	// the caller will be traced, but anything in the callee will be ignored
	// (this is important because otherwise you'd find objects because you were
	// looking for them)
	void *stackBottom = &stackBottom;
	thread->stackBottom = stackBottom;
	// Mark all objects on this thread's stack as visited.
	GCTraceStackSynchronous(thread);

	GCTraceContext *context = calloc(sizeof(GCTraceContext), 1);
	void *
		     valloc(size_t size);
	//GCTraceContext *context = valloc(4096);
	if (thread->cycleBufferInsert)
	{
		context->cycleBuffer = thread->cycleBuffer;
		context->cycleBufferSize = thread->cycleBufferInsert;
		thread->cycleBuffer = calloc(BUFFER_SIZE, sizeof(void*));
		thread->cycleBufferInsert = 0;
	}
	context->forceTrace = forceCollect;
	//mprotect(context, 4096, PROT_READ);
	GCPerform(traceTrampoline, context);
	thread->freeBufferInsert = 0;
}
void GCDrain(BOOL forceCollect)
{
	GCThread *thr = pthread_getspecific(gc_thread_key);
	GCDrainThread(thr, forceCollect);
}
