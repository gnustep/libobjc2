/**
 * Structure identifying regions in memory.  The region is treated as being 
 */
typedef struct
{
	struct gc_buffer_header *buffer;
	void *start;
	void *end;
} GCTracedRegion;


void GCRunTracerIfNeeded(BOOL);

void GCAddObjectsForTracing(GCThread *thr);
void GCTraceStackSynchronous(GCThread *thr);


void GCAddBufferForTracing(struct gc_buffer_header *buffer);

extern volatile int GCGeneration;
void GCCollect();
id objc_assign_strongCast(id obj, id *ptr);
