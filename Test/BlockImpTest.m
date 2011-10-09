#include <objc/runtime.h>
#include <objc/blocks_runtime.h>
#include <assert.h>

@interface Foo @end
@implementation Foo @end
@interface Foo (Dynamic)
+(int)count: (int)i;
@end

int main(void)
{
	__block int b = 0;
	void* blk = ^(id self, int a) {
		b += a; 
		return b; };
	blk = Block_copy(blk);
	IMP imp = imp_implementationWithBlock(blk);
	class_addMethod((objc_getMetaClass("Foo")), @selector(count:), imp, "i@:i");
	assert(2 == [Foo count: 2]);
	assert(4 == [Foo count: 2]);
	assert(6 == [Foo count: 2]);
	assert(imp_getBlock(imp) == (blk));
	imp_removeBlock(blk);
	return 0;
}
