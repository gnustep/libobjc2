/**
 * object.h defines the layout of the object header for GCKit-managed objects.
 * These objects are allocated with GCAllocateObject() and are not freed by
 * code outside of GCKit.
 */

/**
 * GCINLINEPUBLIC functions are functions that are inline for GCKit but
 * exported symbols for the rest of the world.
 */
#ifndef GCINLINEPUBLIC
#define GCINLINEPUBLIC inline static
#endif
/**
 * GCINLINEPRIVATE functions are inline in GCKit and are not exported.
 */
#define GCINLINEPRIVATE inline static __attribute__((unused))

/**
 * Modified version of the object header.  Stores a 16-bit reference count and
 * a 16-bit flags field.  Three bits of the flags are used for the object
 * colour and one to indicate if it is buffered.  
 *
 * Note: On 64-bit platforms we have to add some padding, so it might be better
 * to make the ref countfields bigger.
 */
__attribute__((packed))
struct gc_object_header 
{
	/** 
	 * Garbage collection Flags associated with this object. This includes the
	 * object's colour while performing cycle detection. */
	char    flags;
	/**
	 * Number of weak references held to this object.  An object may be
	 * finalized, but may not be deleted while weak references are held to it.
	 */
	char    weak_ref_count;
	/**
	 * Number of strong references to the object.  This count is modified by
	 * GCRetain() and GCRelease().  When it reaches 0, the object has no strong
	 * references to it.  It may, however, have references from the stack or
	 * traced memory.  When the strong reference count reaches 0, the object
	 * will be added to the trace pile.  
	 */
	short   strong_ref_count;
	/**
	 * The allocation zone for this object.  This is an opaque pointer from the
	 * perspective of GCKit.  In GNUstep, this will be an NSZone.
	 */
	void   *zone;
};

__attribute__((packed))
struct gc_buffer_header
{
	size_t size;
	struct gc_object_header object_header;
};

/**
 * Cycle detection is a graph colouring algorithm.  This type specifies the
 * possible colours.
 */
typedef enum
{
	/** Acyclic */
	GCColourGreen = 0,
	/** In use or free. */
	GCColourBlack = 1,
	/** Possible member of a cycle. */
	GCColourGrey = 2,
	/** Member of a garbage cycle. */
	GCColourWhite = 3,
	/** Potential root of a cycle. */
	GCColourPurple = 4,
	/** Object currently being freed. */
	GCColourOrange = 5,
	/** Object is a member of a cycle to be freed when the last traced
	 * reference is removed, or resurrected if retained. */
	GCColourRed = 6
} GCColour;

typedef enum
{
	/** Set when the object has been added to the potential-garbage list.  */
	GCFlagBuffered    = (1<<3),
	/** Set when an object has been assigned on a traced part of the heap. */
	GCFlagEscaped     = (1<<4),
	/** Visited by the tracing code. */
	GCFlagVisited     = (1<<5),
	/** This object is a memory buffer, not an Objective-C object. */
	GCFlagNotObject   = (1<<6),
	/** Object uses CoreFoundation-style semantics and won't ever by traced. */
	GCFlagCFObject    = (1<<7)
} GCFlag;

/**
 * Debugging function used to return a colour as a human-readable string.
 */
__attribute__((unused))
inline static const char *GCStringFromColour(GCColour aColour)
{
	switch(aColour)
	{
		case GCColourBlack: return "black";
		case GCColourGrey: return "grey";
		case GCColourWhite: return "white";
		case GCColourPurple: return "purple";
		case GCColourGreen: return "green";
		case GCColourOrange: return "orange";
		case GCColourRed: return "red";
	}
	return "unknown";
}
GCINLINEPRIVATE struct gc_object_header*GCHeaderForObject(id anObject)
{
	return &((struct gc_object_header*)anObject)[-1];
}
GCINLINEPRIVATE struct gc_buffer_header*GCHeaderForBuffer(id anObject)
{
	return &((struct gc_buffer_header*)anObject)[-1];
}
/**
 * Returns the flags for a specified object.
 */
GCINLINEPRIVATE unsigned short GCObjectFlags(id anObject)
{
	return GCHeaderForObject(anObject)->flags;
}

/**
 * Returns the colour of the specified object.
 */
GCINLINEPRIVATE GCColour GCColourOfObject(id anObject)
{
	// Lowest 3 bits of the flags field contain the colour.
	return GCObjectFlags(anObject) & 0x7;
}

/**
 * Tries to set the flags for a given object.  Returns the old value.
 */
GCINLINEPRIVATE unsigned short GCTrySetFlags(id anObject, unsigned char old,
		unsigned char value)
{
	return __sync_bool_compare_and_swap(
			&(((struct gc_object_header*)anObject)[-1].flags), old, value);
}
/**
 * Sets the colour of the specified object, returning the old colour
 */
GCINLINEPRIVATE GCColour  GCSetColourOfObject(id anObject, GCColour colour)
{
	char oldFlags;
	char newFlags;
	do
	{
		oldFlags = GCObjectFlags(anObject);
		newFlags = oldFlags;
		// Clear the old colour.
		newFlags &= 0xf8;
		// Set the new colour
		newFlags |= colour;
	} while(!GCTrySetFlags(anObject, oldFlags, newFlags));
	return oldFlags & 0x7;
}

/**
 * Sets the specified flag for a given object.
 */
GCINLINEPRIVATE void GCSetFlag(id anObject, GCFlag flag)
{
	unsigned oldFlags;
	unsigned newFlags;
	do
	{
		oldFlags = GCObjectFlags(anObject);
		newFlags = oldFlags;
		newFlags |= flag;
	} while(!GCTrySetFlags(anObject, oldFlags, newFlags));
}

/**
 * Clears the specified flag on an object.
 */
GCINLINEPRIVATE void GCClearFlag(id anObject, GCFlag flag)
{
	unsigned oldFlags;
	unsigned newFlags;
	do
	{
		oldFlags = GCObjectFlags(anObject);
		newFlags = oldFlags;
		newFlags &= ~flag;
	} while(!GCTrySetFlags(anObject, oldFlags, newFlags));
}

/**
 * Returns whether the specified object's buffered flag is set.
 */
GCINLINEPRIVATE BOOL GCTestFlag(id anObject, GCFlag flag)
{
	return GCObjectFlags(anObject) & flag;
}

GCINLINEPUBLIC long GCGetRetainCount(id anObject)
{
	unsigned short refcount = ((struct gc_object_header*)anObject)[-1].strong_ref_count;
	return (long) refcount;
}
GCINLINEPRIVATE long GCDecrementRetainCount(id anObject)
{
	return __sync_sub_and_fetch(&(GCHeaderForObject(anObject)->strong_ref_count), 1);
}
GCINLINEPRIVATE long GCIncrementRetainCount(id anObject)
{
	return __sync_add_and_fetch(&(GCHeaderForObject(anObject)->strong_ref_count), 1);
}
GCINLINEPUBLIC long GCGetWeakRefCount(id anObject)
{
	unsigned short refcount = ((struct gc_object_header*)anObject)[-1].weak_ref_count;
	return (long) refcount;
}

GCINLINEPRIVATE long GCDecrementWeakCount(id anObject)
{
	return __sync_sub_and_fetch(&(GCHeaderForObject(anObject)->weak_ref_count), 1);
}
GCINLINEPRIVATE long GCIncrementWeakCount(id anObject)
{
	return __sync_add_and_fetch(&(GCHeaderForObject(anObject)->weak_ref_count), 1);
}
