/*
 * Blocks Runtime
 */

#ifdef __cplusplus
#define BLOCKS_EXPORT extern "C"
#else
#define BLOCKS_EXPORT extern 
#endif

BLOCKS_EXPORT void *Block_copy(void *);
BLOCKS_EXPORT void Block_release(void *);
