#import "ModTest.h"

#import "minRepM.h"
#import "minRep1.h"

#import "MyException.h"

#import "stdio.h"

@implementation MinRepM

- (void)poke {
  _mr1 = [MinRep1 new];
  @try {
    printf("Poking from minRepM\n");
    if (_mr1) {
      [_mr1 poke];
    }
    printf("Poked from minRepM\n");
  } @catch (MyException *localException) {
    printf("In NS_HANDLER block, %s %s\n", 
	   [localException name], [localException reason]);
  }
}

@end
