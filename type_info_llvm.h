#ifndef _TYPE_INFO_LLVM_H_INCLUDED_
#define _TYPE_INFO_LLVM_H_INCLUDED_
#include <stdint.h>

namespace __cxxabiv1
{
	struct __class_type_info;
    struct __shim_type_info;
}

namespace std
{
	/**
	 * std::type_info defined with the Clang ABI (for use with libc++). This may
	 * not be exposed in public headers, but is required for correctly implementing
	 * the unified exception model.
	 *
	 * NOTE: libc++ assumes that type_info instances are always layed out by the compiler,
	 * which is not true for libobjc2, where we allocate a static type_info class for
	 * `id' exception handlers. For this reasons, the definition has been modified so
	 * that this is possible, while still allowing vtables to match up correctly.
	 */
	class type_info
	{
				public:
				virtual ~type_info();
				bool operator==(const type_info &) const;
				bool operator!=(const type_info &) const;
				bool before(const type_info &) const;
				type_info();
				private:
				type_info(const type_info& rhs);
				type_info& operator= (const type_info& rhs);
				const char *__type_name;
				protected:
				type_info(const char *name): __type_name(name) { }
				public:
				const char* name() const { return __type_name; }
				// Padding to get subclass vtable to line up
				virtual void noop1() const;
				// Padding to get subnclass vtable to line up
                virtual void noop2() const;
				virtual bool can_catch(const __cxxabiv1::__shim_type_info *thrown_type,
		                       void *&adjustedPtr) const = 0;
	};
}

namespace __cxxabiv1
{
	class __shim_type_info : public std::type_info {
		public:
		virtual ~__shim_type_info();

		virtual bool can_catch(const __shim_type_info *thrown_type,
		                       void *&adjustedPtr) const = 0;
	};
}

#define CXX_TYPE_INFO_CLASS __cxxabiv1::__shim_type_info

#endif //_TYPE_INFO_LLVM_H_INCLUDED_