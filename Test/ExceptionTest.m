#include "Test.h"

#if __cplusplus
#error This is not an ObjC++ test!
#endif

BOOL finallyEntered = NO;
BOOL cleanupRun = NO;
BOOL idRethrown = NO;
BOOL catchallRethrown = NO;
BOOL wrongMatch = NO;

@interface NSString : Test @end
void runCleanup(void *x)
{
	assert(cleanupRun == NO);
	cleanupRun = YES;
}

int throw(void)
{
	@throw [Test new];
}

int finally(void)
{
	__attribute__((cleanup(runCleanup)))
	int x;
	@try { throw(); }
	@finally  { finallyEntered = YES; }
	return 0;
}
int rethrow_id(void)
{
	@try { finally(); }
	@catch(id x)
	{
		assert(object_getClass(x) == [Test class]);
		idRethrown = YES;
		@throw;
	}
	return 0;
}
int rethrow_catchall(void)
{
	@try { rethrow_id(); }
	@catch(...)
	{
		catchallRethrown = YES;
		@throw;
	}
	return 0;
}
int not_matched_catch(void)
{
	@try { rethrow_catchall(); }
	@catch(NSString *s)
	{
		wrongMatch = YES;
	}
	return 0;
}

int main(void)
{
	@try
	{
		rethrow_catchall();
	}
	@catch (id x)
	{
		assert(finallyEntered == YES);
		assert(cleanupRun == YES);
		assert(idRethrown == YES);
		assert(catchallRethrown == YES);
		assert(wrongMatch == NO);
		assert(object_getClass(x) == [Test class]);
		[x dealloc];
	}
	return 0;
}
