void cxx_throw();

int eh_trampoline()
{
	struct X { ~X() {} } x;
	cxx_throw();
	return 0;
}
