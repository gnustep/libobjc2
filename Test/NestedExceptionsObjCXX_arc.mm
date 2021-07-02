#include "Test.h"


id a;
int throwException(void)
{
	@throw a;
}


int main(void)
{
	id e1 = @"e1";
	id e2 = @"e2";
	@try
	{
		a = e1;
		throwException();
	}
	@catch (id x)
	{
		assert(x == e1);
		@try {
			a = e2;
			@throw a;
		}
		@catch (id y)
		{
			assert(y == e2);
		}
	}
	return 0;
}
