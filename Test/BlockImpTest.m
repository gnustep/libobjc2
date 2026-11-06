#include "../objc/runtime.h"
#include "../objc/blocks_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct big
{
	int a, b, c, d, e;
};

#ifdef __has_attribute
#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
#endif
@interface Foo @end
@implementation Foo @end
@interface Foo (Dynamic)
+(int)count: (int)i;
+(struct big)sret;
@end

// Makes an IMP from a block that is still on the stack.
__attribute__((noinline))
static IMP stackBlockIMP(int value)
{
	int (^blk)(id) = ^(id self) { return value; };
	IMP imp = imp_implementationWithBlock(blk);
	// The runtime must keep its own copy: this frame is gone by the time the
	// IMP is called.
	assert(imp_getBlock(imp) != NULL);
	assert(imp_getBlock(imp) != blk);
	return imp;
}

int main(void)
{
	__block Class cls = objc_getClass("Foo");
	__block int b = 0;
	void* blk = ^(id self, int a) {
		assert(self == cls);
		b += a; 
		return b; };
	blk = Block_copy(blk);
	IMP imp = imp_implementationWithBlock(blk);
	char *type = block_copyIMPTypeEncoding_np(blk);
	assert(NULL != type);
	class_addMethod((objc_getMetaClass("Foo")), @selector(count:), imp, type);
	assert(2 == ((int(*)(id,SEL,int))imp)(cls, @selector(count:), 2));
	free(type);
	assert(4 == [Foo count: 2]);
	assert(6 == [Foo count: 2]);
	assert(imp_getBlock(imp) == (blk));
	IMP imp2 = imp_implementationWithBlock(blk);
	assert(imp != imp2);
	imp_removeBlock(imp);
	assert(imp_getBlock(imp) != (blk));

	blk = ^(id self) {
		assert(self == cls);
		struct big b = {1, 2, 3, 4, 5};
		return b;
	};
	blk = Block_copy((blk));
	imp = imp_implementationWithBlock(blk);
	assert(imp && "Can't make sret IMP");
	// imp_getBlock / imp_removeBlock must also work for struct-return blocks.
	assert(imp_getBlock(imp) == blk);
	type = block_copyIMPTypeEncoding_np(blk);
	assert(NULL != type);
	class_addMethod((objc_getMetaClass("Foo")), @selector(sret), imp, type);
	free(type);
	struct big s = [Foo sret];
	assert(s.a == 1);
	assert(s.b == 2);
	assert(s.c == 3);
	assert(s.d == 4);
	assert(s.e == 5);
	imp_removeBlock(imp);
	assert(imp_getBlock(imp) != blk);

	IMP stackImp = stackBlockIMP(7);
	IMP stackImp2 = stackBlockIMP(42);
	assert(7 == ((int(*)(id,SEL))stackImp)(cls, @selector(count:)));
	assert(42 == ((int(*)(id,SEL))stackImp2)(cls, @selector(count:)));
	assert(imp_removeBlock(stackImp));
	assert(imp_removeBlock(stackImp2));
	return 0;
}
