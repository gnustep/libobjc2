#import "MyException.h"

@implementation MyException

- (MyException *)initWithName:(char *)name
                       reason:(char *)reason
{
  if ((self = [super init]) != nil) {
    _name = name;
    _reason = reason;
  }
  return self;
}

+ (void) raise: (char *)name
	format: (char *)reason
{
  MyException *e = [[[MyException alloc] initWithName:name 
					       reason:reason] autorelease];
  [e raise];
}

- (void) dealloc
{
  if (_name) {
    free(_name);
  }
  if (_reason) {
    free(_reason);
  }
}

- (void) raise {
  @throw self;
}

- (char*)name
{
  return _name;
}

- (char*)reason
{
  return _reason;
}

@end
