#import "Test.h"

#import "minRep2.h"

#import "stdio.h"

void poke_objcxx(void)
{
    @try {
      printf("Raising MyException\n");
      Test *e = [Test new];
      @throw e;
    } @catch (Test *localException) {
      printf("Caught - re-raising\n");
      [localException retain];
      localException = [localException autorelease];;
      rethrow(localException);
    } @catch(...) {
      printf("Caught in catchall\n");
    }
}

