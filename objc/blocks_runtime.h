#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif


/*
 * Blocks Runtime
 */
#include "Availability.h"
#ifdef __cplusplus
#define OBJC_BOOL_TYPE bool
#define OBJC_BLOCK_PTR_TYPE void*
#define BLOCKS_EXPORT extern "C"
#else
#define OBJC_BOOL_TYPE _Bool
#define OBJC_BLOCK_PTR_TYPE id
#define BLOCKS_EXPORT extern 
#endif

OBJC_PUBLIC BLOCKS_EXPORT void *_Block_copy(const void *);
OBJC_PUBLIC BLOCKS_EXPORT void _Block_release(const void *);
OBJC_PUBLIC BLOCKS_EXPORT const char *block_getType_np(const void *b) OBJC_NONPORTABLE;

OBJC_PUBLIC BLOCKS_EXPORT OBJC_BOOL_TYPE _Block_has_signature(OBJC_BLOCK_PTR_TYPE);
OBJC_PUBLIC BLOCKS_EXPORT const char * _Block_signature(OBJC_BLOCK_PTR_TYPE);


#define Block_copy(x) ((__typeof(x))_Block_copy((const void *)(x)))
#define Block_release(x) _Block_release((const void *)(x))
