#include <stdio.h>
#include <stdlib.h>
#include "objc/runtime.h"
#include "dwarf_eh.h"
#include "objcxx_eh.h"

void cxx_throw()
{
	throw 1;
}

int eh_trampoline();
uint64_t cxx_exception_class;

namespace
{
inline _Unwind_Reason_Code continueUnwinding(struct _Unwind_Exception *ex,
                                                    struct _Unwind_Context *context)
{
#if defined(__arm__) && !defined(__ARM_DWARF_EH__)
	if (__gnu_unwind_frame(ex, context) != _URC_OK) { return _URC_FAILURE; }
#endif
	return _URC_CONTINUE_UNWIND;
}

bool done_setup;
}

extern "C"
BEGIN_PERSONALITY_FUNCTION(test_eh_personality)
	fprintf(stderr, "Fake EH personality called\n");
	if (!done_setup)
	{
		done_setup = true;
		cxx_exception_class = exceptionClass;
	}
	return CALL_PERSONALITY_FUNCTION(__gxx_personality_v0);
}

extern "C" void test_cxx_eh_implementation()
{
	bool caught = false;
	try
	{
		eh_trampoline();
	}
	catch(int)
	{
		caught = true;
	}
	assert(caught);
}

