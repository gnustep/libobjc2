#import "MyException.h"
#include <stdlib.h>

@implementation MyException

- (MyException *)initWithName:(char *)name
                       reason:(char *)reason
{
  _name = name;
  _reason = reason;
  return self;
}

+ (void) raise: (char *)name
	format: (char *)reason
{
  MyException *e = [[[MyException new] initWithName:name 
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
  [super dealloc];
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
