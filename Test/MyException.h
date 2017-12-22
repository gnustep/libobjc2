#include "ModTest.h"

@interface MyException : Test {
@private
  char *_name;
  char *_reason;
}
+ (void) raise: (char *)name
	format: (char *)reason;
- (void) raise;
- (char *)name;
- (char *)reason;
- (MyException *)initWithName:(char *)name
		       reason:(char *)reason;

@end
