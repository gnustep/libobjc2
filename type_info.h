#ifndef _TYPE_INFO_H_INCLUDED_
#define _TYPE_INFO_H_INCLUDED_
#include <stdint.h>

namespace __cxxabiv1
{
	struct __class_type_info;
#if CXX_STDLIB_IS_GNU != 1
    struct __shim_type_info;
#endif
}

namespace std
{
	/**
	 * std::type_info defined with the GCC ABI.  This may not be exposed in
	 * public headers, but is required for correctly implementing the unified
	 * exception model.
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
#				if CXX_STDLIB_IS_GNU == 1
				virtual bool __is_pointer_p() const;
				virtual bool __is_function_p() const;
				virtual bool __do_catch(const type_info *thrown_type,
				                        void **thrown_object,
				                        unsigned outer) const;
				virtual bool __do_upcast(
				                const __cxxabiv1::__class_type_info *target,
				                void **thrown_object) const;
#				else
				virtual void noop1() const;
                virtual void noop2() const;
				virtual bool can_catch(const __cxxabiv1::__shim_type_info *thrown_type,
		                       void *&adjustedPtr) const = 0;
#				endif

	};
}

#if CXX_STDLIB_IS_GNU == 1
#define CXX_TYPE_INFO_CLASS std::type_info
#else
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
#endif // NOT CXX_STDLIB_IS_GNU
#endif //_TYPE_INFO_H_INCLUDED_