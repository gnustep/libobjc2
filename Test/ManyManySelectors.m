#include "Test.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>


static BOOL methodCalled = NO;

static char selBuffer[] = "XXXXXXXselectorXXXXXXXX";

static id x(id self, SEL _cmd)
{
	methodCalled = YES;
	assert(strcmp(selBuffer, sel_getName(_cmd)) == 0);
	return self;
}

int main(void)
{
	SEL nextSel;
	Class cls = [Test class];
	assert(cls != Nil);
	int sel_size = 0;
	for (uint32_t i=0 ; i<0xf0000 ; i++)
	{
		snprintf(selBuffer, 16, "%" PRId32 "selector%" PRIx32, i, i);
		nextSel = sel_registerName(selBuffer);
		sel_size += strlen(selBuffer);
	}
	assert(class_addMethod(object_getClass([Test class]), nextSel, (IMP)x, "@@:"));
	assert(cls == [Test class]);
	// Test both the C and assembly code paths.
	objc_msg_lookup(cls, nextSel)(cls, nextSel);
	assert(methodCalled == YES);
	methodCalled = NO;
	objc_msgSend([Test class], nextSel);
	assert(methodCalled == YES);
	return 0;
}

