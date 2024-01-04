#include "Test.h"
#include "../objc/hooks.h"
#include "../objc/objc-exception.h"

#include <stdlib.h>

#ifdef _WIN32
#include <Windows.h>
#endif

id exceptionObj = @"Exception";

void _UncaughtExceptionHandler(id exception)
{
	assert(exception == exceptionObj);
#ifdef _WIN32
	// on Windows we will exit in _UnhandledExceptionFilter() below
#else
	exit(0);
#endif
}

#ifdef _WIN32
LONG WINAPI _UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo)
{
	assert(exceptionInfo != NULL);
	exit(0);
}
#endif

int main(void)
{
	#if !(defined(__arm__) || defined(__ARM_ARCH_ISA_A64))
#ifdef _WIN32
	// also verify that an existing handler still gets called after we set ours
	SetUnhandledExceptionFilter(&_UnhandledExceptionFilter);
#endif

	objc_setUncaughtExceptionHandler(_UncaughtExceptionHandler);
	@throw exceptionObj;
	assert(0 && "should not be reached!");

	return -1;
	#endif
	// FIXME: Test currently fails on ARM and AArch64
	return 77; // Skip test
}
