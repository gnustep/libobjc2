
#ifndef __objc_runtime_INCLUDE_GNU
struct objc_class
{
	/**
	 * Pointer to the metaclass for this class.  The metaclass defines the
	 * methods use when a message is sent to the class, rather than an
	 * instance.
	 */
	struct objc_class         *isa;
	/**
	 * Pointer to the superclass.  The compiler will set this to the name of
	 * the superclass, the runtime will initialize it to point to the real
	 * class.
	 */
	struct objc_class         *super_class;
	/**
	 * The name of this class.  Set to the same value for both the class and
	 * its associated metaclass.
	 */
	const char                *name;
	/**
	 * The version of this class.  This is not used by the language, but may be
	 * set explicitly at class load time.
	 */
	long                       version;
	/**
	 * A bitfield containing various flags.  See the objc_class_flags
	 * enumerated type for possible values.  
	 */
	unsigned long              info;
	/**
	 * The size of this class.  For classes using the non-fragile ABI, the
	 * compiler will set this to a negative value The absolute value will be
	 * the size of the instance variables defined on just this class.  When
	 * using the fragile ABI, the instance size is the size of instances of
	 * this class, including any instance variables defined on superclasses.
	 *
	 * In both cases, this will be set to the size of an instance of the class
	 * after the class is registered with the runtime.
	 */
	long                       instance_size;
	/**
	 * Metadata describing the instance variables in this class.
	 */
	struct objc_ivar_list     *ivars;
	/**
	 * Metadata for for defining the mappings from selectors to IMPs.  Linked
	 * list of method list structures, one per class and one per category.
	 */
	struct objc_method_list   *methods;
	/**
	 * The dispatch table for this class.  Intialized and maintained by the
	 * runtime.
	 */
	void                      *dtable;
	/**
	 * A pointer to the first subclass for this class.  Filled in by the
	 * runtime.
	 */
	struct objc_class         *subclass_list;
	/**
	 * A pointer to the next sibling class to this.  You may find all
	 * subclasses of a given class by following the subclass_list pointer and
	 * then subsequently following the sibling_class pointers in the
	 * subclasses.
	 */
	struct objc_class         *sibling_class;

	/**
	 * Metadata describing the protocols adopted by this class.  Not used by
	 * the runtime.
	 */
	struct objc_protocol_list *protocols;
	/**
	 * Pointer used by the Boehm GC.
	 */
	void                      *gc_object_type;
	/**
	* New ABI.  The following fields are only available with classes compiled to
	* support the new ABI.  You may test whether any given class supports this
	* ABI by using the CLS_ISNEW_ABI() macro.
	*/

	/**
	* The version of the ABI used for this class.  This is currently always zero.  
	*/
	long                       abi_version;

	/** 
	* Array of pointers to variables where the runtime will store the ivar
	* offset.  These may be used for faster access to non-fragile ivars if all
	* of the code is compiled for the new ABI.  Each of these pointers should
	* have the mangled name __objc_ivar_offset_value_{class name}.{ivar name}
	*
	* When using the compatible non-fragile ABI, this faster form should only be
	* used for classes declared in the same compilation unit.
	*
	* The compiler should also emit symbols of the form 
	* __objc_ivar_offset_{class name}.{ivar name} which are pointers to the
	* offset values.  These should be emitted as weak symbols in every module
	* where they are used.  The legacy-compatible ABI uses these with a double
	* layer of indirection.
	*/
	int                      **ivar_offsets;
	/**
	* List of declared properties on this class (NULL if none).  This contains
	* the accessor methods for each property.
	*/
	struct objc_property_list *properties;
};
#endif

/**
 * An enumerated type describing all of the valid flags that may be used in the
 * info field of a class.
 */
enum objc_class_flags
{
	/** This class structure represents a class. */
	objc_class_flag_class = (1<<0),
	/** This class structure represents a metaclass. */
	objc_class_flag_meta = (1<<1),
	/**
	 * This class has been sent a +initalize message.  This message is sent
	 * exactly once to every class that is sent a message by the runtime, just
	 * before the first other message is sent.
	 */
	objc_class_flag_initialized = (1<<2),
	/** 
	 * The class has been initialized by the runtime.  Its super_class pointer
	 * should now point to a class, rather than a C string containing the class
	 * name, and its subclass and sibling class links will have been assigned,
	 * if applicable.
	 */
	objc_class_flag_resolved = (1<<3),
	/** 
	 * The class uses the new, Objective-C 2, runtime ABI.  This ABI defines an
	 * ABI version field inside the class, and so will be used for all
	 * subsequent versions that retain some degree of compatibility.
	 */
	objc_class_flag_new_abi = (1<<4),
	/**
	 * This class was created at run time and may be freed.
	 */
	objc_class_flag_user_created = (1<<5),
	/** 
	 * Instances of this class have a reference count and plane ID prepended to
	 * them.  The default for this is set for classes, unset for metaclasses.
	 * It should be cleared by protocols, constant strings, and objects not
	 * allocated by NSAllocateObject().
	 */
	objc_class_flag_plane_aware = (1<<6)
};

static inline void objc_set_class_flag(struct objc_class *aClass,
                                       enum objc_class_flags flag)
{
	aClass->info |= (long)flag;
}
static inline BOOL objc_test_class_flag(struct objc_class *aClass,
                                        enum objc_class_flags flag)
{
	return aClass->info & (long)flag;
}
