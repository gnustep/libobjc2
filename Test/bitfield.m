#include "stdio.h"
#include "Test.h"
#include "objc/runtime.h"
#include "objc/encoding.h"

@interface Bitfield : Test
{
	unsigned short first;
	unsigned  isShip: 1,
	          isStation: 1;
	unsigned y;
}

@end
@implementation Bitfield
- (BOOL)isShip { return isShip; }
- (BOOL)isStation { return isStation; }
- (void)setShip: (BOOL)aValue
{
	isShip = aValue;
}
- (void)setStation: (BOOL)aValue
{
	isStation = aValue;
}
- (void)setY: (int)anInt
{
	y = anInt;
}
@end

static size_t offset(const char *ivar)
{
	return ivar_getOffset(class_getInstanceVariable([Bitfield class], ivar));
}

static size_t size(const char *ivar)
{
	return objc_sizeof_type(ivar_getTypeEncoding(class_getInstanceVariable([Bitfield class], ivar)));
}

int main(void)
{
	Bitfield *bf = [Bitfield new];
	assert(![bf isShip]);
	assert(![bf isStation]);
	[bf setShip: YES];
	assert([bf isShip]);
	assert(![bf isStation]);
	[bf setStation: YES];
	[bf setY: 0];
	assert([bf isShip]);
	assert([bf isStation]);
	assert(offset("isShip") >= offset("first") + size("first"));
	assert(offset("isShip") == offset("isStation"));
	assert(offset("y") >= offset("isStation") + size("isStation"));
}
