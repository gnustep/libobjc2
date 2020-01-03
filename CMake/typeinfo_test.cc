#include <stdint.h>
#include "../type_info.h"

class type_info2 : public std::type_info
{
	public:
	type_info2() : type_info("foo") {}
	virtual bool __is_pointer_p() const; 
	virtual bool __is_function_p() const { return true; }
	virtual bool __do_catch(const type_info *thrown_type,
	                        void **thrown_object,
	                        unsigned outer) const { return true; }
	virtual bool can_catch(const CXX_TYPE_INFO_CLASS *thrown_type,
	                        void *&thrown_object) const { return true; }
	virtual void noop1() const {}
	virtual void noop2() const {}
};
bool type_info2::__is_pointer_p() const { return true; }


int main()
{
	type_info2 s;
	return s.__is_pointer_p();
}
