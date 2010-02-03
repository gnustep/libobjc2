/**
 * Structure identifying regions in memory.  The region is treated as being 
 */
typedef struct
{
	void *start;
	void *end;
} GCTracedRegion;


void GCRunTracerIfNeeded(BOOL);

void GCAddObjectsForTracing(GCThread *thr);
void GCTraceStackSynchronous(GCThread *thr);

extern volatile int GCGeneration;
void GCCollect();
id objc_assign_strongCast(id obj, id *ptr);
