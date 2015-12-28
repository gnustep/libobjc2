#include "Test.h"
#include "../objc/runtime.h"

@interface Foo : Test
{
	@public
	__weak id w;
	__unsafe_unretained id u;
	__strong id s;
}
@end
@implementation Foo @end
@interface Dealloc : Test
@end
int dealloc = 0;
@implementation Dealloc
- (void)dealloc
{
	dealloc++;
}
@end

void setIvar(id obj, const char * name, id val)
{
	object_setIvar(obj, class_getInstanceVariable(object_getClass(obj), name), val);
}

int main(void)
{
	Foo *f = [Foo new];
	Dealloc *d = [Dealloc new];
	__unsafe_unretained Dealloc *dead;
	setIvar(f, "w", d);
	assert(f->w == d);
	assert(dealloc == 0);
	d = 0;
	assert(dealloc == 1);
	assert(f->w == nil);
	dealloc = 0;
	d = [Dealloc new];
	dead = d;
	setIvar(f, "s", d);
	assert(dealloc == 0);
	assert(f->s == d);
	d = nil;
	assert(dealloc == 0);
	assert(f->s == dead);
	setIvar(f, "s", nil);
	assert(dealloc == 1);
	assert(f->s == nil);
	return 0;
}
