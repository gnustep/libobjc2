#import "ModTest.h"

#import "minRepM.h"

#import "stdio.h"

int main (int iArgc, const char *iArgv[])
{
  @autoreleasepool {
    MinRepM *mrm = [MinRepM new];
    printf("Poking\n");
    [mrm poke];
    printf("Poked\n");
  }

  return 0;
}
