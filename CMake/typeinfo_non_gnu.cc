#include <typeinfo>
/*
 * This intentionally subclasses std::type_info in a way that is incompatible
 * with the API exposed by libstdc++, but not libc++. This means that the
 * resulting file will fail to compile under libstdc++, but not libc++.
 */
class type_info2 : public std::type_info
{
	public:
	type_info2() : type_info("foo") {}
	virtual int __is_pointer_p() const; 
};
int type_info2::__is_pointer_p() const { return 123; }


int main()
{
	type_info2 s;
	return s.__is_pointer_p();
}
