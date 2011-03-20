#include <stdlib.h>
#include <stdio.h>
#include "dwarf_eh.h"
#include <typeinfo>
#include <exception>
#include "objcxx_eh.h"
extern "C" 
{
#include "objc/runtime.h"
};


using namespace std;


static BOOL isKindOfClass(Class thrown, Class type)
{
	do
	{
		if (thrown == type)
		{
			return YES;
		}
		thrown = class_getSuperclass(thrown);
	} while (Nil != thrown);

	return NO;
}

/**
 * C++ Exception structure.  From the Itanium ABI spec
 */
struct __cxa_exception
{
	std::type_info *exceptionType;
	void (*exceptionDestructor) (void *);
	unexpected_handler unexpectedHandler;
	terminate_handler terminateHandler;
	__cxa_exception *nextException;
	unsigned int handlerCount;
	int handlerSwitchValue;
	const char *actionRecord;
	const char *languageSpecificData;
	void *catchTemp;
	void *adjustedPtr;
	_Unwind_Exception unwindHeader;
};




namespace gnustep
{
	namespace libobjc
	{
		struct __objc_id_type_info : std::type_info
		{
			__objc_id_type_info() : type_info("@id") {};
			virtual ~__objc_id_type_info();
			virtual bool __do_catch(const type_info *thrownType,
			                        void **obj,
			                        unsigned outer) const;
		};
		struct __objc_class_type_info : std::type_info
		{
			virtual ~__objc_class_type_info();
			virtual bool __do_catch(const type_info *thrownType,
			                        void **obj,
			                        unsigned outer) const;
		};
	}
};

gnustep::libobjc::__objc_class_type_info::~__objc_class_type_info() {}
gnustep::libobjc::__objc_id_type_info::~__objc_id_type_info() {}
bool gnustep::libobjc::__objc_class_type_info::__do_catch(const type_info *thrownType,
                                                          void **obj,
                                                          unsigned outer) const
{
	id thrown = (id)obj;
	bool found = false;
	// Id throw matches any ObjC catch.  This may be a silly idea!
	if (dynamic_cast<const __objc_id_type_info*>(thrownType))
	{
		thrown = **(id**)obj;
		// nil matches any handler
		if (0 == thrown)
		{
			found = true;
		}
		else
		{
			// Check whether the real thrown object matches the catch type.
			found = isKindOfClass(object_getClass(thrown), (Class)objc_getClass(name()));
		}
	}
	if (dynamic_cast<const __objc_class_type_info*>(thrownType))
	{
		thrown = **(id**)obj;
		found = isKindOfClass((Class)objc_getClass(thrownType->name()),
		                      (Class)objc_getClass(name()));
	}
	if (found)
	{
		*obj = (void*)thrown;
	}
	return found;
};

bool gnustep::libobjc::__objc_id_type_info::__do_catch(const type_info *thrownType,
                                                       void **obj,
                                                       unsigned outer) const
{
	// Id catch matches any ObjC throw
	if (dynamic_cast<const __objc_class_type_info*>(thrownType))
	{
		return true;
	}
	if (dynamic_cast<const __objc_id_type_info*>(thrownType))
	{
		return true;
	}
	return false;
};

//static gnustep::libobjc::__objc_id_type_info objc_id_type_info;
extern "C"
{
//gnustep::libobjc::__objc_id_type_info *__objc_id_type_info = &objc_id_type_info;
gnustep::libobjc::__objc_id_type_info __objc_id_type_info;

static void exception_cleanup(_Unwind_Reason_Code reason,
                              struct _Unwind_Exception *ex)
{
	__cxa_exception *cxxex = (__cxa_exception*) ((char*)ex - offsetof(struct __cxa_exception, unwindHeader));
	if (cxxex->exceptionType != &__objc_id_type_info)
	{
		delete cxxex->exceptionType;
	}
	__cxa_free_exception((void*)ex);
}

struct _Unwind_Exception *objc_init_cxx_exception(void *thrown_exception)
{
	__cxa_exception *ex = ((__cxa_exception*)thrown_exception) - 1;

	std::type_info *tinfo = &__objc_id_type_info;

	ex->exceptionType = tinfo;

	ex->exceptionDestructor = 0;

	ex->unwindHeader.exception_class = EXCEPTION_CLASS('G','N','U','C','C','+','+','\0');
	ex->unwindHeader.exception_cleanup = exception_cleanup;

	return &ex->unwindHeader;
}

void* objc_object_for_cxx_exception(void *thrown_exception)
{
	__cxa_exception *ex = (__cxa_exception*) ((char*)thrown_exception -
			offsetof(struct __cxa_exception, unwindHeader));
	const std::type_info *thrownType = ex->exceptionType;
	if (!dynamic_cast<const gnustep::libobjc::__objc_id_type_info*>(thrownType) && 
	    !dynamic_cast<const gnustep::libobjc::__objc_class_type_info*>(thrownType))
	{
		return (id)-1;
	}
	return (id)(ex-1);
}

void print_type_info(void *thrown_exception)
{
	__cxa_exception *ex = (__cxa_exception*) ((char*)thrown_exception -
			offsetof(struct __cxa_exception, unwindHeader));
	fprintf(stderr, "Type info: %s\n", ex->exceptionType->name());
	fprintf(stderr, "offset is: %d\n", offsetof(struct __cxa_exception, unwindHeader));
}

} // extern "C"

