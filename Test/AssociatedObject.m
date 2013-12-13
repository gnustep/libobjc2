#include "Test.h"

static BOOL deallocCalled = NO;
static const char* objc_setAssociatedObjectKey = "objc_setAssociatedObjectKey";

@interface Associated : Test
@end

@implementation Associated
-(void) dealloc
{
    deallocCalled = YES;
    [super dealloc];
}
@end

int main(void)
{
	@autoreleasepool {
		Associated *object = [Associated new];
		Test *holder = [[Test new] autorelease];
		objc_setAssociatedObject(object, &objc_setAssociatedObjectKey, holder, OBJC_ASSOCIATION_RETAIN);
		[object release];
    }
	assert(deallocCalled);
}
