#include "Test.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static BOOL methodCalled = NO;

static id x(id self, SEL _cmd)
{
	methodCalled = YES;
	assert(strcmp("selectoreffff", sel_getName(_cmd)) == 0);
	return self;
}

int main(void)
{
	char selBuffer[] = "selectorXXXXXXXX";
	SEL nextSel;
	Class cls = [Test class];
	assert(cls != Nil);
	for (uint32_t i=0 ; i<0xf0000 ; i++)
	{
		snprintf(selBuffer, 16, "selector%" PRIx32, i);
		nextSel = sel_registerName(selBuffer);
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
