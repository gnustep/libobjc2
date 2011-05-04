#include "objc/runtime.h"
#include "protocol.h"
#include "class.h"
#include <stdio.h>
#include <string.h>

@implementation Protocol
+ (void)load
{
	objc_clear_class_flag(self, objc_class_flag_plane_aware);
}
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
@implementation Protocol2 
+ (void)load
{
	objc_clear_class_flag(self, objc_class_flag_plane_aware);
}
@end

/**
 * This class exists for the sole reason that the legacy GNU ABI did not
 * provide a way of registering protocols with the runtime.  With the new ABI,
 * every protocol in a compilation unit that is not referenced should be added
 * in a category on this class.  This ensures that the runtime sees every
 * protocol at least once and can perform uniquing.
 */
@interface __ObjC_Protocol_Holder_Ugly_Hack { id isa; } @end
@implementation __ObjC_Protocol_Holder_Ugly_Hack @end

@implementation Object @end
