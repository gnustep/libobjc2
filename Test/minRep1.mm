#import "ModTest.h"

#import "minRep1.h"

#import "stdio.h"

@implementation MinRep1

- (void)poke
{
  MinRep2 *mr2 = [MinRep2 new];
  printf("Poking from minRep1\n");
  [mr2 poke];
  [mr2 release];
}

@end
