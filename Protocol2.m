#include "objc/runtime.h"
#include "protocol.h"
#include "class.h"
#include <stdio.h>
#include <string.h>

@implementation Protocol
// FIXME: This needs removing, but it's included for now because GNUstep's
// implementation of +[NSObject conformsToProtocol:] calls it.
- (BOOL)conformsTo: (Protocol*)p
{
	return protocol_conformsToProtocol(self, p);
}
- (id)retain
{
	return self;
}
- (void)release {}
+ (Class)class { return self; }
- (id)self { return self; }
@end
@interface __IncompleteProtocol : Protocol @end
@implementation __IncompleteProtocol @end

@implementation Object @end

@implementation ProtocolGSv1 @end

PRIVATE void link_protocol_classes(void)
{
	[Protocol class];
	[ProtocolGSv1 class];
}
