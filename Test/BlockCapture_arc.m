#include "Test.h"

// Regression test: objc_retain must return the same pointer it was called
// with, regardless of whether the argument is a stack or heap block.  Clang's
// ARC codegen declares llvm.objc.retain with the `returned` attribute; at -O2+
// the optimizer relies on that to replace uses of the retain result with the
// original pointer.  A previous version of libobjc2's objc_retain forwarded
// unconditionally to Block_copy, which for stack blocks allocates a heap copy
// and returns a different pointer.  The optimizer discarded the heap-copy
// pointer and later released the original stack pointer (a no-op for stack
// blocks), so the heap copy and every strong reference it captured leaked.

static int liveCount;

@interface Capture : Test @end
@implementation Capture
+ (id)new
{
	liveCount++;
	return [super new];
}
- (void)dealloc
{
	liveCount--;
}
- (int)value
{
	return 42;
}
@end

// A method (not a plain function) so message dispatch prevents inlining and
// forces the compiler to treat the block parameter as escapable.
@interface Runner : Test @end
@implementation Runner
- (void)runWith:(int (^)(void))b
{
	// Capturing `b` into another block requires ARC to retain the block
	// value — this is the exact site where the bug fires.
	int (^wrapper)(void) = ^{ return b(); };
	(void)wrapper();
}
@end

int main(void)
{
	Runner *r = [Runner new];
	for (int i = 0; i < 100; i++)
	{
		@autoreleasepool
		{
			Capture *c = [Capture new];
			[r runWith:^{ return [c value]; }];
		}
	}
	assert(liveCount == 0);
	return 0;
}
