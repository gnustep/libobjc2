#include "Test.h"
#include "../objc/slot.h"
#include <assert.h>

@interface QualReturn : Test
- (double)d;
- (bycopy double)bcd;
- (float)f;
- (bycopy float)bcf;
@end
@implementation QualReturn
- (double)d { return 1.0; }
- (bycopy double)bcd { return 1.0; }
- (float)f { return 1.0f; }
- (bycopy float)bcf { return 1.0f; }
@end

/* The nil-receiver fast paths in the runtime lookup functions pick a zero slot
 * from the method's return type, so a message to nil returns a correctly typed
 * zero.  The return type is read after skipping any type qualifiers (bycopy,
 * oneway, ...).  A qualifier-prefixed floating-point return (e.g. bycopy
 * double, encoded "Od...") must map to the same zero slot as the plain
 * floating-point return; otherwise nil gets an integer zero slot and the
 * floating-point result is undefined.
 */

static IMP nilSlotSender(SEL sel)
{
	id nilObj = nil;
	return objc_msg_lookup_sender(&nilObj, sel, nil)->method;
}

static IMP nilSlotLookup2(SEL sel)
{
	id nilObj = nil;
	return objc_msg_lookup2(&nilObj, sel);
}

int main(void)
{
	Class cls = [QualReturn class];
	Method mD   = class_getInstanceMethod(cls, @selector(d));
	Method mBcD = class_getInstanceMethod(cls, @selector(bcd));
	Method mF   = class_getInstanceMethod(cls, @selector(f));
	Method mBcF = class_getInstanceMethod(cls, @selector(bcf));
	SEL d   = method_getName(mD);
	SEL bcd = method_getName(mBcD);
	SEL f   = method_getName(mF);
	SEL bcf = method_getName(mBcF);

	/* Premise: the bycopy qualifier prefixes the encoding, so the return type
	 * character is not at offset 0. */
	assert('O' == method_getTypeEncoding(mBcD)[0]);
	assert('O' == method_getTypeEncoding(mBcF)[0]);

	/* A qualifier-prefixed floating-point return must select the same nil slot
	 * as the unqualified one. */
	assert(nilSlotSender(bcd) == nilSlotSender(d));
	assert(nilSlotSender(bcf) == nilSlotSender(f));
	assert(nilSlotLookup2(bcd) == nilSlotLookup2(d));
	assert(nilSlotLookup2(bcf) == nilSlotLookup2(f));

	return 0;
}
