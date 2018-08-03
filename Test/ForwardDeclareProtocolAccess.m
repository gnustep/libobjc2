#include "Test.h"

@protocol P
- (void)someMethod;
@end

// Defined in another compilation unit.  Returns @protocol(P).
Protocol *getProtocol(void);

int main(void)
{
	Protocol *p1 = @protocol(P);
	Protocol *p2 = getProtocol();
	assert(protocol_isEqual(p1, p2));
#ifdef GS_RUNTIME_V2
	// With the new ABI, these should be precisely the same object.
	assert(p1 == p2);
#endif
	return 0;
}
