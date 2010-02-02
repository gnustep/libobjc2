/**
 * Make sure that all inline functions that are part of the public API are emitted.
 */
#include "../objc/runtime.h"
#define GCINLINEPUBLIC
#include "object.h"
