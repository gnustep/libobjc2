#include <stdio.h>
#include <stdlib.h>

// This function is exported as a weak symbol to enable GNUstep or some other
// framework to replace it trivially
void __attribute__((weak)) objc_enumerationMutation(void *obj)
{
	fprintf(stderr, "Mutation occured during enumeration.");
	abort();
}

