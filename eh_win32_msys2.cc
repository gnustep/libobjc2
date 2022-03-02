#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

#include "objc/runtime.h"
#include "visibility.h"

#include <windows.h>

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

namespace
{
} // <anonymous-namespace>

OBJC_PUBLIC extern "C" void objc_exception_rethrow(void* exc);

OBJC_PUBLIC extern "C" void objc_exception_throw(id object)
{
}

OBJC_PUBLIC extern "C" void objc_exception_rethrow(void* exc)
{
}
