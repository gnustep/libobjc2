
/**
 * Metadata structure for an instance variable.
 *
 */
struct objc_ivar
{
	/**
	 * Name of this instance variable.
	 */
	const char *name;
	/**
	 * Type encoding for this instance variable.
	 */
	const char *type;
	/**
	 * The offset from the start of the object.  When using the non-fragile
	 * ABI, this is initialized by the compiler to the offset from the start of
	 * the ivars declared by this class.  It is then set by the runtime to the
	 * offset from the object pointer.
	 */
	int         offset;
	/**
	 * Alignment of this ivar.
	 */
	int         align;
	/**
	 * Flags for this instance variable.
	 */
	int         flags;
};

/**
 * Instance variable ownership.
 */
typedef enum {
	/**
	 * Invalid.  Indicates that this is not an instance variable with ownership
	 * semantics.
	 */
	ownership_invalid = 0,
	/**
	 * Strong ownership.  Assignments to this instance variable should retain
	 * the assigned value.
	 */
	ownership_strong  = 1,
	/**
	 * Weak ownership.  This ivar is a zeroing weak reference to an object.
	 */
	ownership_weak    = 2,
	/**
	 * Object that has `__unsafe_unretained` semantics.
	 */
	ownership_unsafe  = 3
} ivar_ownership;

/**
 * Mask applied to the flags field to indicate ownership.
 */
static const int ivar_ownership_mask = 3;

static inline void ivarSetOwnership(Ivar ivar, ivar_ownership o)
{
	ivar->flags = (ivar->flags & ~ivar_ownership_mask) | o;
}

/**
 * Look up the ownership for a given instance variable.
 */
static inline ivar_ownership ivarGetOwnership(Ivar ivar)
{
	return (ivar_ownership)(ivar->flags & ivar_ownership_mask);
}

/**
 * Legacy ivar structure, inherited from the GCC ABI.
 */
struct objc_ivar_legacy
{
	/**
	 * Name of this instance variable.
	 */
	const char *name;
	/**
	 * Type encoding for this instance variable.
	 */
	const char *type;
	/**
	 * The offset from the start of the object.  When using the non-fragile
	 * ABI, this is initialized by the compiler to the offset from the start of
	 * the ivars declared by this class.  It is then set by the runtime to the
	 * offset from the object pointer.
	 */
	int         offset;
};


/**
 * A list of instance variables declared on this class.  Unlike the method
 * list, this is a single array and size.  Categories are not allowed to add
 * instance variables, because that would require existing objects to be
 * reallocated, which is only possible with accurate GC (i.e. not in C).
 */
struct objc_ivar_list
{
	/**
	 * The number of instance variables in this list.
	 */
	int              count;
	/**
	 * An array of instance variable metadata structures.  Note that this array
	 * has count elements.
	 */
	struct objc_ivar ivar_list[];
};

/**
 * Legacy version of the ivar list
 */
struct objc_ivar_list_legacy
{
	/**
	 * The number of instance variables in this list.
	 */
	int              count;
	/**
	 * An array of instance variable metadata structures.  Note that this array
	 * has count elements.
	 */
	struct objc_ivar_legacy ivar_list[];
};

