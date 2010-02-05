#if 0


////////////////////////////////////////////////////////////////////////////////
// TESTING:
////////////////////////////////////////////////////////////////////////////////

/**
 * Simple object which stores pointers to two objects.  Used to test whether
 * cycle detection is really working by creating garbage cycles and checking
 * that they are free'd.
 */
@interface Pair 
{
	Class isa;
@public
	id a, b;
}
@end
@implementation Pair
/**
 * Create a new pair and enable cycle detection for it.
 */
+ (id) new
{
	id new = GCAllocateObjectWithZone(self, NULL, o);
	// Enable automatic cycle detection for this object.
	setColourOfObject(new, black);
	return new;
}
/**
 * Release both pointers and log that the object has been freed.
 */
- (void) dealloc
{
	fprintf(stderr, "Pair destroyed\n");
	[a release];
	[b release];
	[super dealloc];
}
@end

int main(int argc, char **argv, char **env)
{
	id pool = [GCAutoreleasePool new];
	// FIXME: Test object -> traced region -> object
	Pair * a1 = [Pair new];
	Pair * a2 = [Pair new];
	Pair * a3 = [Pair new];
	Pair * a4 = [Pair new];
	Pair * a5 = [Pair new];
	a1->a = [a2 retain];
	a1->b = [a5 retain];
	a2->a = [a2 retain];
	a2->b = [a4 retain];
	a3->a = [a3 retain];
	a3->b = [a4 retain];
	a4->a = [a3 retain];
	a4->b = [a5 retain];
	a5->a = [a5 retain];
	a5->b = [a1 retain];
	a5->b = [NSObject new];
	visObject(a1, @"Test");
	// Check that we haven't broken anything yet...
	NSLog(@"Testing? %@", a1);
	[a1 release];
	[a2 release];
	[a3 release];
	[a4 release];
	[a5 release];
	//[pool drain];
	[pool release];
	//fprintf(stderr, "Buffered Objects: %d\n", loopBufferInsert);
    return 0;
}
#endif
#include "../objc/runtime.h"
#import "malloc.h"
#import "thread.h"
#import "trace.h"
#import "cycle.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

@interface NSConstantString
{
	id isa;
	char *c_str;
	unsigned len;
}
@end

@interface SimpleObject
{
	Class isa;
}
+ (id)new;
@end

@implementation SimpleObject
+ (id)new
{
	id obj = GCAllocateObjectWithZone(self, NULL, 0);
	return obj;
}
- (void)log
{
	printf("Simple object %x is still alive\n", (int)self);
}
- (void)finalize
{
	printf("%s %x finalised\n", class_getName(isa), (int)self);
}
@end
// The test program calls GCDrain() repeatedly to force the GC to run.  In real
// code, this will be triggered automatically as a result of object allocations
// and reference count changes.  In this code, however, it is not.  The test
// case will exit before the GC would run in normal use.  This is not a bug;
// there's no point spending CPU time collecting objects a few milliseconds
// before the process exits and the OS reclaims them all at once.  The point of
// a garbage collector is to reclaim memory for reuse, and if no reuse is going
// to take place, there is no point reclaiming it.

void makeObject(void)
{
	SimpleObject *foo = [SimpleObject new];
	[foo log];
	GCDrain(YES);
	GCDrain(YES);
	[foo log];
	foo = nil;
	[foo log];
	GCDrain(YES);
}

void doStuff(void)
{
	makeObject();
}

void makeRefCountedObject(void)
{
	SimpleObject *foo = [SimpleObject new];
	GCRelease(GCRetain(foo));
	[foo log];
	GCDrain(YES);
	GCDrain(YES);
}

void doRefCountStuff(void)
{
	makeRefCountedObject();
}

static id *buffer;

void putObjectInBuffer(void)
{
	buffer = (id*)GCRetain((id)GCAllocateBufferWithZone(NULL, sizeof(id), YES));
	buffer[0] = objc_assign_strongCast([SimpleObject new], buffer);
	[*buffer log];
	GCDrain(YES);
	GCDrain(YES);
}

void testTracedMemory(void)
{
	putObjectInBuffer();
	GCDrain(YES);
}
@interface Pair : SimpleObject
{
@public
	Pair *a, *b;
}
@end
@implementation Pair @end

void makeObjectCycle(void)
{
	Pair *obj = [Pair new];
	obj->a = GCRetain([Pair new]);
	obj->b = GCRetain([Pair new]);
	obj->a->a = GCRetain(obj->b);
	obj->b->b = GCRetain(obj->a);
	obj->a->b = GCRetain(obj);
	obj->b->a = GCRetain(obj);
	[obj log];
	GCRelease(GCRetain(obj));
	GCDrain(YES);
}

void testCycle(void)
{
	makeObjectCycle();
	GCDrain(YES);
	GCDrain(YES);
}

void makeTracedCycle(void)
{
	// These two buffers are pointing to each other
	id *b1 = GCAllocateBufferWithZone(NULL, sizeof(id), YES);
	Pair *p = [Pair new];
	id *b2 = GCAllocateBufferWithZone(NULL, sizeof(id), YES);
	fprintf(stderr, "Expected to leak %x and %x\n", (int)b1, (int)b2);
	//objc_assign_strongCast((id)b2, b1);
	objc_assign_strongCast(p, b1);
	objc_assign_strongCast((id)b1, b2);
	p->a = (id)b2;
}

void testTracedCycle(void)
{
	makeTracedCycle();
}

int main(void)
{
	testTracedCycle();
	// Not required on main thread:
	//GCRegisterThread();
	doStuff();
	GCDrain(YES);
	doRefCountStuff();
	GCDrain(YES);
	testTracedMemory();
	buffer[0] = objc_assign_strongCast(nil, buffer);
	GCDrain(YES);

	testCycle();
	GCDrain(YES);
	GCDrain(YES);
	GCDrain(YES);
	sched_yield();
	GCDrain(YES);
	GCDrain(YES);
	printf("Waiting to make sure the GC thread has caught up before the test exits\n");
	sleep(1);
}
