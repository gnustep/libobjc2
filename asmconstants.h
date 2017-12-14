#ifdef __LP64__
#define DTABLE_OFFSET  64
#define SMALLOBJ_BITS  3
#define SHIFT_OFFSET   0
#define DATA_OFFSET    8
#define SLOT_OFFSET    32
#elif (__SIZEOF_LONG__ == 4) && (__SIZEOF_POINTER__ == 8)  /* LLP64 */
#define DTABLE_OFFSET  52   /* 5 pointers + 3 longs */
#define SMALLOBJ_BITS  3
#define SHIFT_OFFSET   0
#define DATA_OFFSET    8
#define SLOT_OFFSET    28   /* 3 pointers + 1 long */
#else
#define DTABLE_OFFSET  32
#define SMALLOBJ_BITS  1
#define SHIFT_OFFSET   0
#define DATA_OFFSET    8
#define SLOT_OFFSET    16
#endif
#define SMALLOBJ_MASK  ((1<<SMALLOBJ_BITS) - 1)
