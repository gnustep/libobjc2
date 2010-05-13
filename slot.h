
/**
 * A slot, stored in the dispatch table.  A pointer to a slot is returned by
 * every lookup function.  The slot may be safely cached.
 */
struct objc_slot
{
	/**
	 * The class that owns this slot.
	 */
	Class owner;
	Class cachedFor;
	const char *types;
	int version;
	IMP method;
};
