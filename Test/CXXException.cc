
extern "C" void throw_int()
{
	throw 12;
}

extern "C" void throw_id();
extern "C" int id_catchall;


extern "C" int catchall()
{
	try
	{
		throw_id();
	}
	catch(...)
	{
		id_catchall = 1;
		throw;
	}
	__builtin_trap();
}
