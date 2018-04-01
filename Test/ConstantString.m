#import "Test.h"
#include <string.h>

@interface NSConstantString : Test
{
	const char * const str;
	const unsigned int len;
}
- (unsigned int)length;
- (const char*)cString;
@end

@implementation NSConstantString
- (unsigned int)length
{
	return len;
}
- (const char*)cString
{
	return str;
}
@end


int main(void)
{
	assert([@"1234567890" length] == 10);
	assert(strcmp([@"123456789" cString], "123456789") == 0);
	return 0;
}
