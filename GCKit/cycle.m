#include "../objc/runtime.h"
#import "object.h"
#import "malloc.h"
#import "thread.h"
#import "visit.h"
#include <stdio.h>

id GCRetain(id anObject)
{
	GCIncrementRetainCount(anObject);
	GCSetFlag(anObject, GCFlagEscaped);
	return anObject;
}
/**
 * Collect garbage cycles.  Inspects every object in the loopBuffer and frees
 * any that are part of garbage cycles.  This is an implementation of the
 * algorithm described in:
 *
 * http://www.research.ibm.com/people/d/dfb/papers/Bacon01Concurrent.pdf
 *
 */
void GCRelease(id anObject)
{
	// If decrementing the strong retain count is 0, the object is probably
	// garbage.  Add it to the list to trace and throw it away if it is.
	if (GCDecrementRetainCount(anObject) <= 0)
	{
		// FIXME: Discard it immediately if it is using CF semantics
		// Mark this object as in-use or free
		GCSetColourOfObject(anObject, GCColourBlack);
		// Clear its buffered flag (we won't look at it again)
		GCClearFlag(anObject, GCFlagBuffered);
		// Add it for freeing if tracing doesn't find any references to it
		GCAddObject(anObject);
	}
	else
	{
		// If this object is not marked as acyclic
		if (GCColourOfObject(anObject) == GCColourGreen)
		{
			// Mark it as the possible root of a cycle.  The object was
			// released, but there are still strong references to it.  That
			// means that it has 
			GCSetColourOfObject(anObject, GCColourPurple);
			GCSetFlag(anObject, GCFlagBuffered);
			GCAddObject(anObject);
		}
	}
}

void GCAddObjectForTracing(id object);

/**
 * Scan children turning them black and incrementing the reference count.  Used
 * for objects which have been determined to be acyclic.
 */
static void GCScanBlackChild(id anObject, void *unused, BOOL isWeak)
{
	GCIncrementRetainCount(anObject);
	if (GCColourOfObject(anObject) != GCColourBlack)
	{
		GCSetColourOfObject(anObject, GCColourBlack);
		GCVisitChildren(anObject, GCScanBlackChild, NULL, NO);
	}
}

/**
 * Scan objects turning them black if they are not part of a cycle and white if
 * they are.
 */
static void GCScan(id anObject, void* unused, BOOL isWeak)
{
	GCColour colour = GCColourOfObject(anObject);
	// If the object is not grey, then we've visited it already.
	if (colour == GCColourGrey)
	{
//fprintf(stderr, "%x has retain count of %d\n", (int)anObject, (int)GCGetRetainCount(anObject));
		// If the retain count is still > 0, we didn't account for all of the
		// references with cycle detection, so mark it as black and reset the
		// retain count of every object that it references.
		//
		// If it did reach 0, then this is part of a garbage cycle so colour it
		// accordingly.  Any objects reachable from this object do not get
		// their reference counts restored.
		//
		// FIXME: We need to be able to resurrect objects if they are
		// GCRetain()'d when they are white
		if (GCGetRetainCount(anObject) > 0)
		{
			GCSetColourOfObject(anObject, GCColourBlack);
			GCVisitChildren(anObject, GCScanBlackChild, NULL, NO);
		}
		else
		{
			GCSetColourOfObject(anObject, GCColourWhite);
			GCVisitChildren(anObject, GCScan, NULL, NO);
		}
	}
}

/**
 * Collect objects which are coloured white.
 *
 * In the original algorithm, white objects were collected immediately.  In
 * this version, it's possible that they have traced pointers referencing them,
 * so we defer collection.  We can only collect a garbage cycle when there are
 * no traced pointers to any of the nodes.
 */
static void GCCollectWhite(id anObject, void *ignored, BOOL isWeak)
{
	//fprintf(stderr, "Looking at object %x with colour %s\n", (unsigned) anObject, [GCStringFromColour(GCColourOfObject(anObject)) UTF8String]);
	if ((GCColourOfObject(anObject) == GCColourWhite))
	{
		GCSetColourOfObject(anObject, GCColourRed);
		//fprintf(stderr, "%x marked red.  Red's dead, baby!\n", (int)anObject);
		//fprintf(stderr, " has refcount %d!\n", (int)GCGetRetainCount(anObject));
		GCAddObjectForTracing(anObject);
		GCVisitChildren(anObject, GCCollectWhite, NULL, NO);
	}
}

/**
 * Mark objects grey if are not already grey.
 * 
 * Grey indicates that an object is possibly a member of a cycle.  We check
 * that by traversing all reachable objects from the potential root of a cycle,
 * decrementing their reference count, and marking them grey.  If the reference
 * count drops to 0, it indicates that all of the strong references to this
 * object come from cycles.
 */
void GCMarkGreyChildren(id anObject, void *ignored, BOOL isWeak)
{
	//fprintf(stderr, "Marking %x as grey\n", (int)anObject);
	// FIXME: This should probably check if the colour is green.  Green objects
	// can't be parts of cycles, and we need to restore the green colour after
	// scanning anyway.
	GCDecrementRetainCount(anObject);
	if (GCColourOfObject(anObject) != GCColourGrey)
	{
		GCSetColourOfObject(anObject, GCColourGrey);
		GCVisitChildren(anObject, GCMarkGreyChildren, NULL, NO);
	}
}

void GCScanForCycles(id *loopBuffer, unsigned count)
{
	//fprintf(stderr, "Starting to detect cycles...\n");
	// Mark Roots
	id next;
	for (unsigned i=0 ; i<count ; i++)
	{
		next = loopBuffer[i];
		//fprintf(stderr, "Looking at %x\n", (int)next);
		// Check that this object is still eligible for cycle detection
		if (nil == next) continue;
		if (GCTestFlag(next, GCFlagNotObject)) continue;
		if (!GCTestFlag(next, GCFlagBuffered))
		{
			loopBuffer[i] = nil;
			continue;
		}
		GCColour colour = GCColourOfObject(next);
		// If this is the potential root of a cycle (which it might not be
		// anymore, if something else has changed its colour)
		if (colour == GCColourPurple)
		{
			// Mark it, and all of its children, as grey.
			//fprintf(stderr, "Marking grey: %d...\n", colour);
			GCSetColourOfObject(next, GCColourGrey);
			GCVisitChildren(next, GCMarkGreyChildren, nil, NO);
		}
		else
		{
			GCClearFlag(next, GCFlagBuffered);
			// If the object's refcount is 0, add it to the list to free if the
			// tracer can't find them.
			if ((colour == GCColourBlack) && (GCGetRetainCount(next) <= 0))
			{
				GCAddObjectForTracing(next);
			}
			loopBuffer[i] = nil;
		}
	}
	// Scan roots
	for (unsigned i=0 ; i<count ; i++)
	{
		next = loopBuffer[i];
		if (nil == next) continue;
		//fprintf(stderr, "scanning object...\n");
		GCScan(next, NULL, NO);
	}

	for (unsigned i=0 ; i<count ; i++)
	{
		next = loopBuffer[i];
		if (nil == next) continue;
		GCCollectWhite(next, NULL, NO);
	}
	void GCRunTracerIfNeeded(BOOL);
	GCRunTracerIfNeeded(YES);
}

#if 0
// Code from the old GCKit for drawing pretty pictures.
// FIXME: Make it draw pretty pictures with the new GCKit too.
/**
 * Table of objects that have already been visualised.
 */
NSHashTable __thread drawnObjects;
/**
 * Recursively output connections from this object in GraphViz .dot format.
 */
void vizGraph(id self, SEL _cmd, NSString *parent)
{
	NSString *me = [NSString stringWithFormat:@"object%d", (unsigned)self];
	if (NULL != NSHashGet(drawnObjects, self))
	{
		if (nil != parent)
		{
			printf("\t%s -> %s\n", [parent UTF8String], [me UTF8String]);
		}
		return;
	}
	// Add the node:
	if (GCColourOfObject(self) == black)
	{
		printf("\t%s [style=filled, fillcolor=black, fontcolor=white, label=\"%s\"]\n", [me UTF8String], self->class_pointer->name);
	}
	else
	{
		printf("\t%s [style=filled, fillcolor=%s, label=\"%s\"]\n", [me UTF8String], [GCStringFromColour(GCColourOfObject(self)) UTF8String], self->class_pointer->name);
	}
	// Add the connection to the parent
	if (nil != parent)
	{
		printf("\t%s -> %s\n", [parent UTF8String], [me UTF8String]);
	}
	NSHashInsert(drawnObjects, self);
	for_all_children(self, (IMP)vizGraph, _cmd, me);
}
/**
 * Print a GraphViz-compatible graph of all objects reachable from this one and
 * their colours.
 */
void visObject(id object, NSString *graphName)
{
	drawnObjects = NSCreateHashTable(NSNonOwnedPointerHashCallBacks, 100);
	printf("digraph %s {\n", [graphName UTF8String]);
	vizGraph(object, @selector(vizGraph:), nil);
	printf("}\n");
	NSFreeHashTable(drawnObjects);
}
#endif
