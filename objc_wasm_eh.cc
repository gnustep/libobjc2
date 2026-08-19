#include <cxxabi.h>
#include <typeinfo>
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "visibility.h"

/* We build libobjc2 for WebAssembly with emscripten against the libc++abi.
 * For now, we do not define a custom personality function and instead let Objective-C
 * exceptions piggyback on C++ exceptions.
 *
 * For WebAssembly clang defaults to `__gxx_wasm_personality_v0` (as of August 2026),
 * which redirects to `__gxx_personality_imp` (The C++ unwind phase handler).
 * We therefore supply our own RTTI object derived from c++abi's internal
 * type info class to override the `can_catch` method and handle unboxing/catching of
 * Objective-C Exceptions correctly.
 */
namespace __cxxabiv1
{

// (Re)declare libc++abi's type info class, so we can derive from it.
// This is a private implementation detail of libc++abi, so we need to keep it in sync.
class __shim_type_info : public std::type_info
{
	protected:
	explicit __shim_type_info(const char *name) : type_info(name) {}

	public:
	virtual ~__shim_type_info();
	virtual void noop1() const;
	virtual void noop2() const;
	virtual bool can_catch(const __shim_type_info *thrownType,
		void *&adjustedPtr) const = 0;
};
}

namespace
{
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

static id thrownObject(void *pointer)
{
	return *static_cast<id *>(pointer);
}

static void *releaseThrownObject(void *pointer)
{
	objc_release(thrownObject(pointer));
	return pointer;
}
}  // namespace

namespace gnustep {
namespace libobjc {
struct OBJC_PUBLIC __objc_type_info : __cxxabiv1::__shim_type_info
{
	explicit __objc_type_info(const char *name) : __shim_type_info(name) {}
};

struct OBJC_PUBLIC __objc_id_type_info : __objc_type_info
{
	__objc_id_type_info() : __objc_type_info("@id") {}
	bool can_catch(const __cxxabiv1::__shim_type_info *thrownType,
		void *&adjustedPtr) const override;
};

struct OBJC_PUBLIC __objc_class_type_info : __objc_type_info
{
	using __objc_type_info::__objc_type_info;
	bool can_catch(const __cxxabiv1::__shim_type_info *thrownType,
		void *&adjustedPtr) const override;
};

bool __objc_id_type_info::can_catch(
	const __cxxabiv1::__shim_type_info *thrownType, void *&adjustedPtr) const
{
	if (dynamic_cast<const __objc_type_info *>(thrownType) == nullptr)
	{
		return false;
	}
	adjustedPtr = thrownObject(adjustedPtr);
	return true;
}

bool __objc_class_type_info::can_catch(
	const __cxxabiv1::__shim_type_info *thrownType, void *&adjustedPtr) const
{
	if (dynamic_cast<const __objc_type_info *>(thrownType) == nullptr)
	{
		return false;
	}
	// TODO: Apple compat/non-compat behavior
	id thrown = thrownObject(adjustedPtr);
	Class caughtClass = (Class)objc_getClass(name());
	if (nil == thrown || Nil == caughtClass ||
		!isKindOfClass(object_getClass(thrown), caughtClass))
	{
		return false;
	}
	adjustedPtr = thrown;
	return true;
}  // namespace libobjc
}  // namespace gnustep

extern "C"
{
OBJC_PUBLIC gnustep::libobjc::__objc_id_type_info __objc_id_type_info;

static bool AppleCompatibleMode = true;
OBJC_PUBLIC int objc_set_apple_compatible_objcxx_exceptions(int newValue)
{
	bool oldValue = AppleCompatibleMode;
	AppleCompatibleMode = newValue;
	return oldValue;
}

OBJC_PUBLIC __attribute__((noreturn)) void objc_exception_throw(id object)
{
	objc_retain(object);
	id *exception = static_cast<id *>(
		__cxxabiv1::__cxa_allocate_exception(sizeof(id)));
	*exception = object;
	__cxxabiv1::__cxa_throw(exception, &__objc_id_type_info,
		releaseThrownObject);
}

OBJC_PUBLIC __attribute__((noreturn)) void objc_exception_rethrow(void)
{
	__cxxabiv1::__cxa_rethrow();
}
}
