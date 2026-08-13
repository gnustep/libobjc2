#include "Test.h"
#include <string.h>

// Registering a typed selector whose untyped form is already known reads the
// selector table while the registration holds the table's lock.  Both
// registrations answer the same name, and the typed one keeps its types.
int main(void)
{
	SEL untyped = sel_registerName("probeMethodWithValue:");
	assert(strcmp(sel_getName(untyped), "probeMethodWithValue:") == 0);
	assert(sel_getType_np(untyped) == NULL);

	SEL typed = sel_registerTypedName_np("probeMethodWithValue:", "v@:i");
	assert(strcmp(sel_getName(typed), "probeMethodWithValue:") == 0);
	assert(strcmp(sel_getType_np(typed), "v@:i") == 0);
	// One copy of the name is kept, shared with the untyped selector.
	assert(sel_getName(typed) == sel_getName(untyped));

	// The same pair in the other order.
	SEL typedFirst = sel_registerTypedName_np("otherProbeMethod:", "v@:d");
	SEL untypedSecond = sel_registerName("otherProbeMethod:");
	assert(strcmp(sel_getName(untypedSecond), "otherProbeMethod:") == 0);
	assert(strcmp(sel_getType_np(typedFirst), "v@:d") == 0);

	return 0;
}
