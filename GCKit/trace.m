/**
 * trace.m implements the tracing part of the collector.  This is responsible
 * for checking for references to objects on stacks and in traced regions on
 * the heap before finally freeing them.
 */
#include <stdlib.h>
#include <sys/limits.h>
#include <assert.h>
#include <stdio.h>
#include <dlfcn.h>
#include "../objc/runtime.h"
#import "object.h"
#import "thread.h"
#import "trace.h"
#import "cycle.h"
#import "malloc.h"
#import "static.h"
#import "workqueue.h"

/**
 * Structure storing pointers that are currently being traced.
 *
 * We store the last location where an object was seen so that, when no objects
 * are found, we can 
 *
 * TODO: We currently don't use the last-seen addresses for objects.  If we
 * did, we could quickly verify that the last reference to them was still valid
 * and eliminate some 
 */
typedef struct
{
	/** The object that might be ready to be freed. */
	id pointer;
	/** Last on-heap address that we saw for this object. */
	id *heapAddress;
	/** Highest on-stack address that we saw for this object. */
	id *stackAddress;
	/** Sweep pass number when this object's visited flag was cleared.  All
	 * traced regions - stack and heap - must have been traced at least once
	 * before this object can be freed. */
	int visitClearedGeneration;
} GCTracedPointer;

// HUGE FIXME: Handle wrapping of this sensibly.
volatile int GCGeneration;

static const GCTracedPointer GCNullTracedPointer = {0,0};

/**
 * Pointer comparison.  Needed for the hash table.
 */
static int traced_pointer_compare(const void *a, const GCTracedPointer b)
{
	return a == b.pointer;
}
/**
 * Pointer hash function.  The lowest bits of a pointer have very little
 * entropy - we have lots of objects the same size and alignment, so they will
 * end up at the same place within a page.
 */
static int traced_pointer_hash(const GCTracedPointer obj)
{
	intptr_t ptr = (intptr_t)obj.pointer;
	return (ptr >> 4) | (ptr << 4);
}
static int traced_pointer_key_hash(const void *obj)
{
	intptr_t ptr = (intptr_t)obj;
	return (ptr >> 4) | (ptr << 4);
}
static int traced_pointer_is_null(const GCTracedPointer obj)
{
	return obj.pointer == NULL;
}
#define MAP_TABLE_NAME traced_object
#define MAP_TABLE_COMPARE_FUNCTION traced_pointer_compare
#define MAP_TABLE_HASH_VALUE traced_pointer_hash
#define MAP_TABLE_HASH_KEY traced_pointer_key_hash
#define MAP_TABLE_VALUE_TYPE GCTracedPointer
#define MAP_TABLE_VALUE_NULL traced_pointer_is_null
#define MAP_TABLE_VALUE_PLACEHOLDER GCNullTracedPointer
#define MAP_TABLE_NO_LOCK

#include "../hash_table.h"
/**
 * Pointer comparison.  Needed for the hash table.
 */
static int pointer_compare(const void *a, const void *b)
{
	return a == b;
}
#define MAP_TABLE_NAME unescaped_object 
#define MAP_TABLE_COMPARE_FUNCTION pointer_compare
#define MAP_TABLE_HASH_KEY traced_pointer_key_hash
#define MAP_TABLE_HASH_VALUE traced_pointer_key_hash
#define MAP_TABLE_NO_LOCK
#define MAP_TABLE_SINGLE_THREAD

#include "../hash_table.h"

static traced_object_table *traced_objects;
/**
 * Read write lock for modifying the traced object set.  The GC thread may read
 * from the tree without acquiring this lock, but other threads must acquire a
 * read lock before reading from it.  Any thread must acquire the write lock
 * before modifying the traced object set.  Only the GC thread may remove
 * objects, other threads may modify them.
 */
static pthread_rwlock_t traced_objects_lock;

typedef struct _GCTracedRegionTreeNode
{
	GCTracedRegion region;
	struct _GCTracedRegionTreeNode *child[2];
	enum { RED, BLACK=0 } colour;
} GCTracedRegionTreeNode;

/**
 * Root of a red-black tree used to store regions that are traced.  Note that
 * this is not protected by any locks.  We ensure serialisation by doing both
 * tracing and freeing of traced regions in the same thread.  
 *
 * Red-black implementation based on Julienne Walker's public domain version.
 */
static GCTracedRegionTreeNode *GCRegionTreeRoot;
/**
 * Compare two traced regions and return a value that can be compared to 0 to
 * find their ordering.
 */
static int GCCompareRegions(GCTracedRegion region1, GCTracedRegion region2)
{
	// Region 1 is before region 2
	if (region1.end < region2.start)
	{
		return -1;
	}
	// Region 2 is before region 1
	if (region1.start < region2.end)
	{
		return 1;
	}
	// Regions overlap
	return 0;
}

static GCTracedRegion mergeRegions(
		GCTracedRegion region1, GCTracedRegion region2)
{
	if (region1.start > region2.start)
	{
		region1.start = region2.start;
	}
	if (region1.end < region2.end)
	{
		region1.end = region2.end;
	}
	return region1;
}



static GCTracedRegionTreeNode *GCTracedRegionTreeNodeCreate(GCTracedRegion region)
{
	GCTracedRegionTreeNode *node = calloc(1, sizeof(GCTracedRegionTreeNode));
	node->region = region;
	node->colour = RED;
	return node;
}

static int isNodeRed(GCTracedRegionTreeNode *node)
{
	return (node != NULL) && node->colour;
}

static GCTracedRegionTreeNode *rotateTree(GCTracedRegionTreeNode *node,
		int direction)
{
	GCTracedRegionTreeNode *save = node->child[!direction];

	node->child[!direction] = save->child[direction];
	save->child[direction] = node;

	node->colour = RED;
	save->colour = BLACK;

	return save;
}

static GCTracedRegionTreeNode *rotateTreeDouble(GCTracedRegionTreeNode *node,
		int direction)
{
	node->child[!direction] = rotateTree(node->child[!direction], !direction);
	return rotateTree(node, direction);
}

/**
 * Check the red-black tree is really a red black tree and not a nonsense tree.
 */
static int debugTree(GCTracedRegionTreeNode *node)
{
#ifdef DEBUG
	if (NULL == node)
	{
		return 1;
	}
	GCTracedRegionTreeNode *left = node->child[0];
	GCTracedRegionTreeNode *right = node->child[1];
 
     /* Consecutive red childs */
	if (isNodeRed(node) )
	{
		assert(!(isNodeRed(left) || isNodeRed(right)) && "Red violation" );
	}

	/* Invalid binary search tree */
	assert(left == NULL || (GCCompareRegions(left->region, node->region) < 0));
	assert(right == NULL || (GCCompareRegions(right->region, node->region) > 0));

	int leftHeight = debugTree(left);
	int rightHeight = debugTree(right);

	//assert(leftHeight == 0 || rightHeight ==0 || leftHeight == rightHeight);

	/* Only count black children */
	if (leftHeight != 0 && rightHeight != 0)
	{
		return isNodeRed(node) ? leftHeight : leftHeight + 1;
	}
	return 0;
#endif //DEBUG
}


/**
 * Recursively inserts a region into the correct location.
 */
static GCTracedRegionTreeNode *tracedRegionInsert(
		GCTracedRegionTreeNode *root, GCTracedRegion region)
{
	if (NULL == root)
	{
		return GCTracedRegionTreeNodeCreate(region);
	}
	int child = GCCompareRegions(root->region, region);
	// If the regions overlap, just merge them.  Note that this will only
	// affect the structure of the tree if things have already gone badly wrong
	// somewhere else, because memory regions can not be extended into already
	// allocated regions unless you broke something.
	if (child == 0)
	{
		root->region = mergeRegions(root->region, region);
		return root;
	}
	// If root->region < region, child is -1.  Make it 0 for this case and let
	// it remain 1 for the other case.  This gives us the index of the child
	child = child > 0;
	root->child[child] = tracedRegionInsert(root->child[child], region);
	if (isNodeRed(root->child[child]))
	{
		if (isNodeRed(root->child[!child]))
		{
			root->colour = RED;
			root->child[0]->colour = BLACK;
			root->child[1]->colour = BLACK;
		}
		else
		{
			if (isNodeRed(root->child[child]->child[child]))
			{
				root = rotateTree(root, !child);
			}
			else
			{
				root = rotateTreeDouble(root, !child);
			}
		}
	}
	return root;
}

/**
 * Inserts the new region into the tree.
 */
__attribute__((unused))
static void GCTracedRegionInsert(GCTracedRegion region)
{
	GCRegionTreeRoot = tracedRegionInsert(GCRegionTreeRoot, region);
	GCRegionTreeRoot->colour = BLACK;
	debugTree(GCRegionTreeRoot);
}

__attribute__((unused))
static void GCTracedRegionDelete(GCTracedRegion region)
{
	if (GCRegionTreeRoot == NULL)
	{
		return;
	}
	GCTracedRegionTreeNode head = {{0}}; /* False tree root */
	GCTracedRegionTreeNode *q, *p, *g; /* Helpers */
	GCTracedRegionTreeNode *f = NULL;  /* Found item */
	int dir = 1;

	/* Set up helpers */
	q = &head;
	g = p = NULL;
	q->child[1] = GCRegionTreeRoot;

	/* Search and push a red down */
	while ( q->child[dir] != NULL ) 
	{
		int last = dir;

		/* Update helpers */
		g = p, p = q;
		q = q->child[dir];
		dir = GCCompareRegions(q->region, region) < 0;

		/* Save found node */
		if (GCCompareRegions(q->region, region) == 0)
		{
			f = q;
		}

		/* Push the red node down */
		if (!isNodeRed(q) && !isNodeRed(q->child[dir]))
		{
			if (isNodeRed (q->child[!dir]))
			{
				p = p->child[last] = rotateTree(q, dir);
			}
			else if (!isNodeRed(q->child[!dir])) 
			{
				GCTracedRegionTreeNode *s = p->child[!last];

				if (s != NULL) 
				{
					if (!isNodeRed(s->child[!last]) && !isNodeRed(s->child[last]))
					{
						/* Color flip */
						p->colour = 0;
						s->colour = 1;
						q->colour = 1;
					}
					else 
					{
						int dir2 = g->child[1] == p;

						if (isNodeRed(s->child[last]))
						{
							g->child[dir2] = rotateTreeDouble(p, last);
						}
						else if (isNodeRed(s->child[!last]))
						{
							g->child[dir2] = rotateTree(p, last);
						}

						/* Ensure correct coloring */
						q->colour = g->child[dir2]->colour = RED;
						g->child[dir2]->child[0]->colour = BLACK;
						g->child[dir2]->child[1]->colour = BLACK;
					}
				}
			}
		}
	}

	/* Replace and remove if found */
	if (f != NULL)
	{
		f->region = q->region;
		p->child[p->child[1] == q] =
		q->child[q->child[0] == NULL];
		free(q);
	}

	/* Update root and make it black */
	GCRegionTreeRoot = head.child[1];
	if (GCRegionTreeRoot != NULL)
	{
		GCRegionTreeRoot->colour = BLACK;
	}
	debugTree(GCRegionTreeRoot);
}


typedef void(*gc_region_visitor)(GCTracedRegion, void*);

static void GCVisitTracedRegion(GCTracedRegionTreeNode *node,
		gc_region_visitor visitor, void *context)
{
	visitor(node->region, context);
	if (node->child[0])
	{
		GCVisitTracedRegion(node->child[0], visitor, context);
	}
	if (node->child[1])
	{
		GCVisitTracedRegion(node->child[1], visitor, context);
	}
}

void GCVisitTracedRegions(gc_region_visitor visitor, void *context)
{
	if (GCRegionTreeRoot)
	{
		GCVisitTracedRegion(GCRegionTreeRoot, visitor, context);
	}
}


__attribute__((constructor))
static void GCTraceInitialise(void)
{
	traced_objects = traced_object_create(128);
	pthread_rwlock_init(&traced_objects_lock, NULL);
}


struct GCTraceContext
{
	int foundObjects;
};

static void GCTraceRegion(GCTracedRegion region, void *c)
{
	struct GCTraceContext *context = c;
	// Stop if we've already found references to everything that might be
	// garbage.
	id *object = region.start;
	fprintf(stderr, "Region starts at %x (%d bytes)\n", (int)object, (int)region.end - (int)region.start);
	while (object < (id*)region.end)
	{
		if (context->foundObjects == traced_objects->table_used)
		{
			return;
		}
		GCTracedPointer *foundObject = traced_object_table_get(traced_objects, *object);
		if (foundObject && foundObject->pointer)
		{
			//fprintf(stderr, "Found traced heap pointer to %x\n", (int)foundObject->pointer);
			if(!GCTestFlag(foundObject->pointer, GCFlagVisited))
			{
				context->foundObjects++;
				GCSetFlag(foundObject->pointer, GCFlagVisited);
			}
		}
		object++;
	}
}
/**
 * Traces the current thread's stack.
 */
void GCTraceStackSynchronous(GCThread *thr)
{
	//fprintf(stderr, "Scanning the stack...\n");
	int generation = GCGeneration;
	pthread_rwlock_rdlock(&traced_objects_lock);
	if (NULL == thr->unescapedObjects)
	{
		thr->unescapedObjects = unescaped_object_create(256);
	}
	else
	{
		struct unescaped_object_table_enumerator *e = NULL;
		id ptr;
		while ((ptr = unescaped_object_next(thr->unescapedObjects, &e)))
		{
			GCClearFlag(ptr, GCFlagVisited);
		}
	}
	id *object = thr->stackBottom;
	while (object < (id*)thr->stackTop)
	{
		if (unescaped_object_table_get(thr->unescapedObjects, *object))
		{
			// Note: This doesn't actually have to use atomic ops; this object
			// is guaranteed, at this point, not to be referenced by another
			// thread.
			GCSetFlag(*object, GCFlagVisited);
			//fprintf(stderr, "Tracing found %x\n", (int)*object);
		}
		GCTracedPointer *foundObject =
			traced_object_table_get(traced_objects, *object);
		// FIXME: This second test should not be required.  Why are we being
		// returned pointers to NULL?
		if (foundObject && foundObject->pointer)
		{
			if(!GCTestFlag(foundObject->pointer, GCFlagVisited))
			{
				GCSetFlag(foundObject->pointer, GCFlagVisited);
				if (foundObject->stackAddress)
				{
					if ((foundObject->stackAddress < (id*)thr->stackTop &&
						foundObject->stackAddress > object))
					{
						foundObject->stackAddress = object;
					}
				}
				else
				{
					// Record this address if there isn't an existing stack
					// address.
					foundObject->stackAddress = object;
				}
			}
		}
		object++;
	}
	pthread_rwlock_unlock(&traced_objects_lock);

	struct unescaped_object_table_enumerator *e = NULL;
	id ptr;
	while ((ptr = unescaped_object_next(thr->unescapedObjects, &e)))
	{
		id oldPtr;
		// Repeat on the current enumerator spot while we are are deleting things.
		do
		{
			oldPtr = ptr;
			if (!GCTestFlag(ptr, GCFlagVisited))
			{
				GCFreeObject(ptr);
				unescaped_object_remove(thr->unescapedObjects, ptr);
			}
		} while ((oldPtr != (ptr = unescaped_object_current(thr->unescapedObjects, &e)))
				&& ptr);
	}
	thr->scannedInGeneration = generation;
}

void GCRunTracer(void)
{
	struct GCTraceContext context = {0};
	// Mark any objects that we can see as really existing
	GCVisitTracedRegions(GCTraceRegion, &context);
	// Free any objects that we couldn't find references for
	struct traced_object_table_enumerator *e = NULL;
	int threadGeneration = INT_MAX;
	for (GCThread *thr = GCThreadList ; thr != NULL ; thr = thr->next)
	{
		int thrGeneration = thr->scannedInGeneration;
		if (thr->scannedInGeneration < threadGeneration)
		{
			threadGeneration = thrGeneration;
		}
	}
	GCTracedPointer *object;
	while ((object = traced_object_next(traced_objects, &e)))
	{
		GCTracedPointer *oldPtr;
		// Repeat on the current enumerator spot while we are are deleting things.
		do
		{
			oldPtr = object;
			//fprintf(stderr, "Thinking of freeing %x. Visited: %d, clear gen: %d, thread gen: %d\n", (int)object->pointer, GCTestFlag(object->pointer, GCFlagVisited), object->visitClearedGeneration , threadGeneration);
			// If an object hasn't been visited and we have scanned everywhere
			// since we cleared its visited flag, delete it.  This works
			// because the heap write barrier sets the visited flag.
			if (!GCTestFlag(object->pointer, GCFlagVisited) &&
				object->visitClearedGeneration < threadGeneration)
			{
				GCFreeObjectUnsafe(object->pointer);
				traced_object_remove(traced_objects, object->pointer);
			}
		} while (oldPtr != ((object = traced_object_current(traced_objects, &e)))
				&& object);
	}
}

void GCRunTracerIfNeeded(BOOL forceCollect)
{
	struct traced_object_table_enumerator *e = NULL;
	GCTracedPointer *ptr;
	// See if we can avoid running the tracer.  
	while ((ptr = traced_object_next(traced_objects, &e)))
	{
		id object = ptr->pointer;
		// Throw away any objects that are referenced by the heap
		if (GCGetRetainCount(object) > 0 &&
			GCColourOfObject(object) != GCColourRed)
		{
			// Make sure that the retain count is still > 0.  If not then it
			// may have been released but not added to the tracing list
			// (because it was marked for tracing already)
			if (GCGetRetainCount(object) > 0 && 
				GCColourOfObject(object) != GCColourRed)
			{
				pthread_rwlock_wrlock(&traced_objects_lock);
				traced_object_remove(traced_objects, object);
				pthread_rwlock_unlock(&traced_objects_lock);
				continue;
			}
		}
		if (GCTestFlag(object, GCFlagVisited))
		{
			//fprintf(stderr, "Clearing visited flag for %x\n", (int)object);
			GCClearFlag(object, GCFlagVisited);
			ptr->visitClearedGeneration = GCGeneration;
		}
	}
	//fprintf(stderr, "Incrementing generation\n");
	// Invalidate all stack scans.
	GCGeneration++;
	// Only actually run the tracer if we have more than a few objects that
	// might need freeing.  No point killing the cache just to reclaim one or
	// two objects...
	if (traced_objects->table_used > 256 || forceCollect)
	{
		GCRunTracer();
	}
}

/**
 * Adds an object for tracing that the cycle detector has decided needs freeing.
 */
void GCAddObjectForTracing(id object)
{
	if (!traced_object_table_get(traced_objects, object))
	{
		//fprintf(stderr, "Cycle detector nominated %x for tracing\n", (int)object);
		GCTracedPointer obj = {object, 0, 0, 0};
		traced_object_insert(traced_objects, obj);
	}
}

void GCAddObjectsForTracing(GCThread *thr)
{
	id *buffer = thr->freeBuffer;
	unsigned int count = thr->freeBufferInsert;
	//unsigned int generation = GCHeapScanGeneration;
	// No locking is needed for this table, because it is always accessed from
	// the same thread
	if (NULL == thr->unescapedObjects)
	{
		thr->unescapedObjects = unescaped_object_create(256);
	}
	unescaped_object_table *unescaped = thr->unescapedObjects;

	pthread_rwlock_wrlock(&traced_objects_lock);
	for (unsigned int i=0 ; i<count ; i++)
	{
		id object = buffer[i];
		if (!GCObjectIsDynamic(object))
		{
			return;
		}
		// Skip objects that have a strong retain count > 0.  They are
		// definitely still referenced...
		if (GCGetRetainCount(object) > 0 &&
			GCColourOfObject(object) != GCColourRed)
		{
			// ...but they might have become part of a cycle
			GCSetFlag(object, GCFlagBuffered);
			GCSetColourOfObject(object, GCColourPurple);
			GCScanForCycles(&object, 1);
			continue;
		}
		if (GCTestFlag(object, GCFlagEscaped))
		{
			// FIXME: Check if the object is already there, don't add it again
			// if it is, but do update its generation.  It was seen by
			// something in this thread, so it might still be on the stack
			// here, or have been moved to the heap.
			if (!traced_object_table_get(traced_objects, object))
			{
				GCTracedPointer obj = {object, 0, 0, 0};
				traced_object_insert(traced_objects, obj);
				// Make sure that this object is not in the thread's list as well.
				unescaped_object_remove(unescaped, object);
			}
		}
		else
		{
			if (!unescaped_object_table_get(unescaped, object))
			{
				unescaped_object_insert(unescaped, object);
			}
		}
	}
	pthread_rwlock_unlock(&traced_objects_lock);
}

static void GCAddBufferForTracingTrampoline(void *b)
{
	struct gc_buffer_header *buffer = b;
	GCTracedRegion region = { buffer, (char*)buffer + sizeof(struct gc_buffer_header),
		(char*)buffer + sizeof(struct gc_buffer_header) + buffer->size };
	fprintf(stderr, "Buffer has size %d (%d)\n", buffer->size, (int)region.end - (int)region.start);
	GCTracedRegionInsert(region);
}

void GCAddBufferForTracing(struct gc_buffer_header *buffer)
{
	GCPerform(GCAddBufferForTracingTrampoline, buffer);
}

// TODO: memmove_collectable does this for a whole region, but only does the
// locking once.
id objc_assign_strongCast(id obj, id *ptr)
{
	BOOL objIsDynamic = GCObjectIsDynamic(obj);
	// This object is definitely stored somewhere, so mark it as visited
	// for now.  
	if (objIsDynamic && obj)
	{
		GCSetFlag(obj, GCFlagVisited);
		// Tracing semantics do not apply to objects with CF semantics, so skip the
		// next bits if the CF flag is set.
		if (obj && !GCTestFlag(obj, GCFlagCFObject))
		{
			// Don't free this just after scanning the stack. 
			GCSetFlag(obj, GCFlagEscaped);
		}
	}
	pthread_rwlock_wrlock(&traced_objects_lock);
	GCTracedPointer *old = traced_object_table_get(traced_objects, *ptr);
	if (old)
	{
		// If the value that we are overwriting is a traced pointer and this is
		// the pointer to it that we are tracking then mark it as not visited.
		//
		// This object may still have been copied to a stack.  If it hasn't
		// been copied to this stack, then we can collect it in future if it
		// isn't in any other heap blocks?
		if (old->heapAddress == ptr)
		{
			old->heapAddress = 0;
			old->visitClearedGeneration = GCGeneration + 1;
			GCClearFlag(*ptr, GCFlagVisited);
		}
	}
	if (objIsDynamic && obj)
	{
		GCTracedPointer *new = traced_object_table_get(traced_objects, obj);
		if (new)
		{
			new->heapAddress = ptr;
		}
	}
	pthread_rwlock_unlock(&traced_objects_lock);
	*ptr = obj;
	return obj;
}
