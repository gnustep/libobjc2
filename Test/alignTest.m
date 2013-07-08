#include "stdio.h"
#include "Test.h"

// This is a large vector type, which the compiler will lower to some sequence
// of vector ops on the target, or scalar ops if there is no vector FPU.
typedef double __attribute__((vector_size(32))) v4d;

@interface X : Test
{
	id f;
	id g;
}
@end
@implementation X @end

@interface Vector : X
{
	v4d x;
}
@end
@implementation Vector
+ (Vector*)alloc
{
	Vector *v = class_createInstance(self, 0);
	// The initialisation might be done with memset, but will probably be a
	// vector load / store and so will likely fail if x is incorrectly aligned.
	v->x = (v4d){1,2,3,4};
	return v;
}
- (void)permute
{
	// This will become a sequence of one or more vector operations.  We must
	// have the correct alignment for x, even after the instance variable
	// munging, or this will break.
	x *= (v4d){2,3,4,5};
}
@end

int main(void)
{
	[[Vector alloc] permute];
}
