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

@interface SimpleObject
{
	Class isa;
}
+ (id)new;
@end

@implementation SimpleObject
+ (id)new
{
	return GCAllocateObjectWithZone(self, NULL, 0);
}
- (void)log
{
	fprintf(stderr, "Simple object is still alive\n");
}
- (void)finalize
{
	fprintf(stderr, "Simple object finalised\n");
}
@end

void makeObject(void)
{
	SimpleObject *foo = [SimpleObject new];
	fprintf(stderr, "foo (%x) is at stack address %x\n", (int)foo, (int)&foo);
	[foo log];
	GCDrain(YES);
	GCDrain(YES);
	sleep(1);
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
	buffer = GCAllocateBufferWithZone(NULL, sizeof(id), YES);
	buffer[0] = objc_assign_strongCast([SimpleObject new], buffer);
	[*buffer log];
	GCDrain(YES);
	GCDrain(YES);
	sleep(1);
}

void testTracedMemory(void)
{
	putObjectInBuffer();
	GCDrain(YES);
}

int main(void)
{
	// Not required on main thread:
	//GCRegisterThread();
	doStuff();
	GCDrain(YES);
	sleep(2);
	doRefCountStuff();
	GCDrain(YES);
	sleep(2);
	testTracedMemory();
	buffer[0] = objc_assign_strongCast(nil, buffer);
	GCDrain(YES);
}
