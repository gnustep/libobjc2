#include "Test.h"

// A block that captures a stack block copies it when it is copied itself,
// and releases that copy when it goes away. What the captured block
// captured must go with it.

static int deallocs;

@interface Captured : Test
@end
@implementation Captured
- (void)dealloc
{
	deallocs++;
}
@end

static id kept;

static void keep(void (^block)(void))
{
	kept = ^{ block(); };
}

int main(void)
{
	@autoreleasepool
	{
		Captured *captured = [Captured new];
		keep(^{ (void)captured; });
		captured = nil;
		assert(deallocs == 0);
		kept = nil;
	}
	assert(deallocs == 1);
	return 0;
}
