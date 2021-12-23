#include "Test.h"
#include "../objc/hooks.h"

#include <stdlib.h>

id exceptionObj = @"Exception";

void UncaughtExceptionHandler(id exception)
{
	assert(exception == exceptionObj);
	exit(0);
}

int main(void)
{
	_objc_unexpected_exception = UncaughtExceptionHandler;
	@throw exceptionObj;
	assert(0 && "should not be reached!");
	return -1;
}
