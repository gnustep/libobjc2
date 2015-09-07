#include "Test.h"

int main(void)
{
	BOOL caught_exception = NO;
	@try
	{
		@throw(nil);
	}
	@catch (id x)
	{
		assert(nil == x);
		caught_exception = YES;
	}
	assert(caught_exception == YES);
	return 0;
}
