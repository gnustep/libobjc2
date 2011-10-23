#include <time.h>
#include <stdio.h>
#include <objc/runtime.h>
#include <assert.h>
#include <string.h>
#include <class.h>

id objc_msgSend(id, SEL, ...);

typedef struct { int a,b,c,d,e; } s;
s objc_msgSend_sret(id, SEL, ...);

Class TestCls;
@interface Test { id isa; }@end
@implementation Test 
- foo
{
	assert((id)1 == self);
	assert(strcmp("foo", sel_getName(_cmd)) == 0);
	return (id)0x42;
}
+ foo
{
	assert(TestCls == self);
	assert(strcmp("foo", sel_getName(_cmd)) == 0);
	return (id)0x42;
}
+ (s)sret
{
	assert(TestCls == self);
	assert(strcmp("sret", sel_getName(_cmd)) == 0);
	s st = {1,2,3,4,5};
	return st;
}
+ nothing { return 0; }
@end
int main(void)
{
	TestCls = objc_getClass("Test");
	objc_msgSend(TestCls, @selector(nothing));
	objc_msgSend(TestCls, @selector(missing));
	assert(0 == objc_msgSend(0, @selector(nothing)));
	id a = objc_msgSend(objc_getClass("Test"), @selector(foo));
	assert((id)0x42 == a);
	a = objc_msgSend(TestCls, @selector(foo));
	assert((id)0x42 == a);
	objc_registerSmallObjectClass_np(objc_getClass("Test"), 1);
	a = objc_msgSend((id)01, @selector(foo));
	assert((id)0x42 == a);
	s ret = objc_msgSend_sret(TestCls, @selector(sret));
	assert(ret.a == 1);
	assert(ret.b == 2);
	assert(ret.c == 3);
	assert(ret.d == 4);
	assert(ret.e == 5);
#ifdef BENCHMARK
	clock_t c1, c2;
	c1 = clock();
	for (int i=0 ; i<100000000 ; i++)
	{
		[TestCls nothing];
	}
	c2 = clock();
	printf("Traditional message send took %f seconds. \n", 
		((double)c2 - (double)c1) / (double)CLOCKS_PER_SEC);
	c1 = clock();
	for (int i=0 ; i<100000000 ; i++)
	{
		objc_msgSend(TestCls, @selector(nothing));
	}
	c2 = clock();
	printf("objc_msgSend() message send took %f seconds. \n", 
		((double)c2 - (double)c1) / (double)CLOCKS_PER_SEC);
	IMP nothing = objc_msg_lookup(TestCls, @selector(nothing));
	c1 = clock();
	for (int i=0 ; i<100000000 ; i++)
	{
		nothing(TestCls, @selector(nothing));
	}
	c2 = clock();
	printf("Direct IMP call took %f seconds. \n", 
		((double)c2 - (double)c1) / (double)CLOCKS_PER_SEC);
#endif
	return 0;
}
