#pragma once
#include <stdlib.h>

template<typename T>
T *allocate_zeroed(size_t extraSpace = 0)
{
	return static_cast<T*>(calloc(1, sizeof(T) + extraSpace));
}

template<typename T>
T *allocate_zeroed_array(size_t elements)
{
	return static_cast<T*>(calloc(elements, sizeof(T)));
}


