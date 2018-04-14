#import "Test.h"

id __weak var;

@interface ARC : Test @end
@implementation ARC
- (id)loadWeak
{
	return var;
}
- (void)setWeakFromWeak: (id __weak)anObject
{
	var = anObject;
}
- (void)setWeak: (id)anObject
{
	var = anObject;
}
@end


int main(void)
{
	ARC *obj = [ARC new];
	{
		id o1 = [Test new];
		id o2 = [Test new];
		[obj setWeak: o1];
		assert([obj loadWeak] == o1);
		[obj setWeakFromWeak: o2];
		assert([obj loadWeak] == o2);
	}
	assert([obj loadWeak] == nil);
	return 0;
}
