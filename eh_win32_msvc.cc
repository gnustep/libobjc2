#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "objc/runtime.h"
#include "visibility.h"

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if !__has_builtin(__builtin_unreachable)
#define __builtin_unreachable abort
#endif

struct _MSVC_TypeDescriptor
{
	const void* pVFTable;
	void* spare;
	char name[0];
};

struct _MSVC_CatchableType
{
	unsigned int flags;
	_MSVC_TypeDescriptor* type;
	int mdisp;
	int pdisp;
	int vdisp;
	int size;
	void* copyFunction;
};

struct _MSVC_CatchableTypeArray
{
	int count;
	_MSVC_CatchableType* types[0];
};

struct _MSVC_ThrowInfo
{
	unsigned int attributes;
	void* pfnUnwind;
	void* pfnForwardCompat;
	_MSVC_CatchableTypeArray* pCatchableTypeArray;
};

#if defined(_WIN64)
extern "C" int __ImageBase;
#define IMAGE_RELATIVE(ptr) ((decltype(ptr))(ptr ? ((uintptr_t)ptr - (uintptr_t)&__ImageBase) : (uintptr_t)nullptr))
#else
#define IMAGE_RELATIVE(ptr) (ptr)
#endif

extern "C" void __stdcall _CxxThrowException(void*, _MSVC_ThrowInfo*);

namespace
{

static std::string mangleObjcObject()
{
	// This mangled name doesn't vary based on bitness.
	return ".PAUobjc_object@@";
}

static std::string mangleStructNamed(const char* className)
{
	// 32-bit:
	//  .PAUxxx@@ = ?? struct xxx * `RTTI Type Descriptor'
	// 64-bit:
	//  .PEAUxxx@@ = ?? struct xxx * __ptr64 `RTTI Type Descriptor'
	return
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
		std::string(".PEAU") +
#else
		std::string(".PAU") +
#endif
		className + "@@";
}

void fillCatchableType(_MSVC_CatchableType* exceptType)
{
	exceptType->flags = 1;
	exceptType->mdisp = 0;
	exceptType->pdisp = -1;
	exceptType->vdisp = 0;
	exceptType->size = sizeof(id);
	exceptType->copyFunction = nullptr;
}

} // <anonymous-namespace>

PUBLIC extern "C" void objc_exception_throw(id object)
{
	// This is the base vtable for all RTTI entries
	static const void* typeid_vtable = *(void**)&typeid(void *);

	SEL rethrow_sel = sel_registerName("rethrow");
	if ((nil != object) &&
		(class_respondsToSelector(object_getClass(object), rethrow_sel)))
	{
		IMP rethrow = objc_msg_lookup(object, rethrow_sel);
		rethrow(object, rethrow_sel);
		// Should not be reached!  If it is, then the rethrow method actually
		// didn't, so we throw it normally.
	}

	SEL processException_sel = sel_registerName("_processException");
	if ((nil != object) &&
		(class_respondsToSelector(object_getClass(object), processException_sel)))
	{
		IMP processException = objc_msg_lookup(object, processException_sel);
		processException(object, processException_sel);
	}

	// The 'id' base type will be taking up a spot in the list:
	size_t typeCount = 1;

	// Get count of all types in exception
	for (Class cls = object_getClass(object); cls != nil; cls = class_getSuperclass(cls), ++typeCount)
		;

	// Unfortunately we can't put this in a real function since the alloca has to be in this stack frame:
#define CREATE_TYPE_DESCRIPTOR(desc, symName) \
	desc = reinterpret_cast<_MSVC_TypeDescriptor*>(alloca(sizeof(_MSVC_TypeDescriptor) + symName.size() + 1 /* null terminator */)); \
	desc->pVFTable = typeid_vtable; \
	desc->spare = nullptr; \
	strcpy_s(desc->name, symName.size() + 1, symName.c_str());

	auto exceptTypes =
		(_MSVC_CatchableTypeArray*)_alloca(sizeof(_MSVC_CatchableTypeArray) + sizeof(_MSVC_CatchableType*) * typeCount);
	exceptTypes->count = typeCount;

	// Add exception type and all base types to throw information
	size_t curTypeIndex = 0;
	for (Class cls = object_getClass(object); cls != nil; cls = class_getSuperclass(cls))
	{
		auto exceptType = (_MSVC_CatchableType*)_alloca(sizeof(_MSVC_CatchableType));
		fillCatchableType(exceptType);

		auto mangledName = mangleStructNamed(class_getName(cls));
		CREATE_TYPE_DESCRIPTOR(exceptType->type, mangledName);
		exceptType->type = IMAGE_RELATIVE(exceptType->type);
		exceptTypes->types[curTypeIndex++] = IMAGE_RELATIVE(exceptType);
	}

	// Add id (struct objc_object*)
	auto exceptType = (_MSVC_CatchableType*)_alloca(sizeof(_MSVC_CatchableType));
	fillCatchableType(exceptType);
	auto idName = mangleObjcObject();
	CREATE_TYPE_DESCRIPTOR(exceptType->type, idName);
	exceptType->type = IMAGE_RELATIVE(exceptType->type);
	exceptTypes->types[curTypeIndex++] = IMAGE_RELATIVE(exceptType);

	_MSVC_ThrowInfo ti = {
		0, // attributes
		NULL, // pfnUnwind
		NULL, // pfnForwardCompat
		IMAGE_RELATIVE(exceptTypes) // pCatchableTypeArray
	};
	_CxxThrowException(&object, &ti);
	__builtin_unreachable();
}

PUBLIC extern "C" void objc_exception_rethrow(void* exc)
{
	_CxxThrowException(nullptr, nullptr);
	__builtin_unreachable();
}
