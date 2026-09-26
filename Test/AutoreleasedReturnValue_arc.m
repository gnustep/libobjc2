#include <assert.h>
#include "Test.h"

// A getter synthesized for a nonatomic strong property returns its ivar
// without retaining it, and its caller still calls
// objc_retainAutoreleasedReturnValue().  That must retain the value, not
// take over an autorelease of the same object that was made for another
// reason and happens to be on top of the pool.

static int deallocated;

@interface Canary : Test
@end
@implementation Canary
- (void)dealloc
{
	deallocated++;
}
@end

@interface Holder : Test
@property (nonatomic, strong) id value;
@end
@implementation Holder
- (id)storeValue: (__autoreleasing id *)out
{
	*out = self.value;
	return self.value ? nil : self;
}
@end

int main(void)
{
	__autoreleasing id out = nil;
	@autoreleasepool
	{
		Holder *holder = [Holder new];
		holder.value = [Canary new];
		[holder storeValue: &out];
		holder = nil;
		assert(deallocated == 0);
	}
	assert(deallocated == 1);
	return 0;
}
