#import "ModTest.h"

#import "minRep2.h"
#import "MyException.h"

#import "stdio.h"

@implementation MinRep2

- (void)poke {
    @try {
      printf("Raising MyException\n");
      MyException *e = [MyException new];
      @throw e;
    } @catch (MyException *localException) {
      printf("Caught - re-raising\n");
      [localException retain];
      [[localException autorelease] raise];
    } @catch(...) {
      printf("Caught in general block\n");
    }
}

@end
