#import "../objc/runtime.h"
#import "../objc/objc-arc.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#ifndef SINGLE_FILE_TEST
#define SINGLE_FILE_TEST 1
#endif
#include "Test.h"

@implementation NSConstantString @end
