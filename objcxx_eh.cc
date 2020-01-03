typedef struct objc_object* id;
#include <stdlib.h>
#include <stdio.h>
#include "dwarf_eh.h"
#include "objcxx_eh.h"
#include <atomic>

#include "objc/runtime.h"
#include "type_info.h"

extern "C"
void __cxa_throw(void *thrown_exception, std::type_info *tinfo,
                 void (*dest)(void *));

extern "C"
void *__cxa_current_primary_exception();


// Define some C++ ABI types here, rather than including them.  This prevents
// conflicts with the libstdc++ headers, which expose only a subset of the
// type_info class (the part required for standards compliance, not the
// implementation details).

typedef void (*unexpected_handler)();
typedef void (*terminate_handler)();

using namespace std;

static std::atomic<ptrdiff_t> exception_object_offset;
static std::atomic<ptrdiff_t> exception_type_offset;


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




namespace gnustep
{
	namespace libobjc
	{
		struct __objc_id_type_info : std::type_info
		{
			__objc_id_type_info() : type_info("@id") {};
			virtual ~__objc_id_type_info();
			virtual void noop1() const {};
            virtual void noop2() const {};
			virtual bool __do_catch(const type_info *thrownType,
			                        void **obj,
			                        unsigned outer) const;
			virtual bool can_catch(const CXX_TYPE_INFO_CLASS *thrownType,
			                       void *&obj) const;
		};
		struct __objc_class_type_info : std::type_info
		{
			virtual ~__objc_class_type_info();
			virtual void noop1() const {};
            virtual void noop2() const {};
			virtual bool __do_catch(const type_info *thrownType,
			                        void **obj,
			                        unsigned outer) const;
			virtual bool can_catch(const CXX_TYPE_INFO_CLASS *thrownType,
			                       void *&obj) const;
		};
	}
};


static bool AppleCompatibleMode = true;
extern "C" int objc_set_apple_compatible_objcxx_exceptions(int newValue)
{
	bool old = AppleCompatibleMode;
	AppleCompatibleMode = newValue;
	return old;
}

gnustep::libobjc::__objc_class_type_info::~__objc_class_type_info() {}
gnustep::libobjc::__objc_id_type_info::~__objc_id_type_info() {}
bool gnustep::libobjc::__objc_class_type_info::__do_catch(const type_info *thrownType,
                                                          void **obj,
                                                          unsigned outer) const
{
	id thrown = nullptr;
	bool found = false;
	// Id throw matches any ObjC catch.  This may be a silly idea!
	if (dynamic_cast<const __objc_id_type_info*>(thrownType)
	    || (AppleCompatibleMode && 
	        dynamic_cast<const __objc_class_type_info*>(thrownType)))
	{
		thrown = **(id**)obj;
		// nil only matches id catch handlers in Apple-compatible mode, or when thrown as an id
		if (0 == thrown)
		{
			return false;
		}
		// Check whether the real thrown object matches the catch type.
		found = isKindOfClass(object_getClass(thrown),
		                      (Class)objc_getClass(name()));
	}
	else if (dynamic_cast<const __objc_class_type_info*>(thrownType))
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

bool gnustep::libobjc::__objc_class_type_info::can_catch(const CXX_TYPE_INFO_CLASS *thrownType,
                                                          void *&obj) const
{
	return __do_catch(thrownType, &obj, 0);
}

bool gnustep::libobjc::__objc_id_type_info::__do_catch(const type_info *thrownType,
                                                       void **obj,
                                                       unsigned outer) const
{
	// Id catch matches any ObjC throw
	if (dynamic_cast<const __objc_class_type_info*>(thrownType))
	{
		*obj = **(id**)obj;
		return true;
	}
	if (dynamic_cast<const __objc_id_type_info*>(thrownType))
	{
		*obj = **(id**)obj;
		return true;
	}
	return false;
};

bool gnustep::libobjc::__objc_id_type_info::can_catch(const CXX_TYPE_INFO_CLASS *thrownType,
                                                          void *&obj) const
{
	return __do_catch(thrownType, &obj, 0);
}

/**
 * Public interface to the Objective-C++ exception mechanism
 */
extern "C"
{
/**
 * The public symbol that the compiler uses to indicate the Objective-C id type.
 */
gnustep::libobjc::__objc_id_type_info __objc_id_type_info;

struct _Unwind_Exception *objc_init_cxx_exception(id obj)
{
	void *cxxexception = nullptr;
	try
	{
		id *exception_object = static_cast<id*>(__cxa_allocate_exception(sizeof(id)));
		*exception_object = obj;
		__cxa_throw(exception_object, &__objc_id_type_info, nullptr);
	}
	catch (...)
	{
		cxxexception = __cxa_current_primary_exception();
	}
	assert(cxxexception);
	uint64_t *ehcls = reinterpret_cast<uint64_t*>(cxxexception);
	ehcls--;
	int count = 1;
	while (*ehcls != cxx_exception_class)
	{
		ehcls--;
		count++;
		assert((count < 8) && "Exception structure appears to be corrupt");
	}
	ptrdiff_t displacement = reinterpret_cast<const char*>(cxxexception) - reinterpret_cast<const char*>(ehcls);
	assert((exception_object_offset == 0) || (exception_object_offset == displacement));

	exception_object_offset = displacement;

	std::type_info **ehtype = reinterpret_cast<std::type_info**>(ehcls);
	ehtype--;
	count = 1;
	while (*ehtype != &__objc_id_type_info)
	{
		ehtype--;
		count++;
		assert((count < 32) && "Exception structure appears to be corrupt");
	}
	displacement = reinterpret_cast<const char*>(ehtype) - reinterpret_cast<const char*>(ehcls);
	assert((exception_type_offset == 0) || (exception_type_offset == displacement));

	exception_type_offset = displacement;
	return reinterpret_cast<_Unwind_Exception*>(ehcls);
}

void* objc_object_for_cxx_exception(void *thrown_exception, int *isValid)
{
	ptrdiff_t type_offset = exception_type_offset;
	if (type_offset == 0)
	{
		*isValid = 0;
		return nullptr;
	}
	const std::type_info *thrownType = 
	*reinterpret_cast<const std::type_info**>(reinterpret_cast<char*>(thrown_exception) + type_offset);
	if (!dynamic_cast<const gnustep::libobjc::__objc_id_type_info*>(thrownType) && 
	    !dynamic_cast<const gnustep::libobjc::__objc_class_type_info*>(thrownType))
	{
		*isValid = 0;
		return 0;
	}
	*isValid = 1;
	return *reinterpret_cast<id*>(reinterpret_cast<char*>(thrown_exception) + exception_object_offset);
}

/*
void print_type_info(void *thrown_exception)
{
	__cxa_exception *ex = (__cxa_exception*) ((char*)thrown_exception -
			offsetof(struct __cxa_exception, unwindHeader));
	fprintf(stderr, "Type info: %s\n", ex->exceptionType->name());
	fprintf(stderr, "offset is: %d\n", offsetof(struct __cxa_exception, unwindHeader));
}
*/

} // extern "C"

