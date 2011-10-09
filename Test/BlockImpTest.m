#include <objc/runtime.h>
#include <objc/blocks_runtime.h>
#include <assert.h>
#include <stdio.h>

struct big
{
	int a, b, c, d, e;
};

@interface Foo @end
@implementation Foo @end
@interface Foo (Dynamic)
+(int)count: (int)i;
+(struct big)sret;
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

	blk = ^(id self) {
		struct big b = {1, 2, 3, 4, 5};
		return b;
	};
	imp = imp_implementationWithBlock(blk);
	char *type;
	asprintf(&type, "%s@:", @encode(struct big));
	class_addMethod((objc_getMetaClass("Foo")), @selector(sret), imp, type);
	struct big s = [Foo sret];
	fprintf(stderr, "%d %d %d %d %d\n", s.a, s.b, s.c, s.d, s.e);
	return 0;
}
