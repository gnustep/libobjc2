#include "visibility.h"
#include <exception>

#ifdef __GLIBCXX__
/**
 * libsupc++ doesn't expose __cxa_current_primary_exception, so implement this
 * using the libstdc++ wrapper.  The exception pointer in the
 * `std::exception_ptr` object is reference counted, so stealing it by poking
 * at the pointer directly means that we acquire it with a reference count of
 * 1.
 */
PRIVATE extern "C" void *__cxa_current_primary_exception()
{
	std::exception_ptr p = std::current_exception();
	void *obj = *(void**)&p;
	*(void**)&p = nullptr;
	return obj;
}
#endif
