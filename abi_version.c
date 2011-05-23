#include "visibility.h"
#include "objc/runtime.h"
#include "module.h"
#include "gc_ops.h"
#include <assert.h>
#include <stdio.h>

/**
 * The smallest ABI version number of loaded modules.
 */
static unsigned long min_loaded_version;
/**
 * The largest ABI version number of loaded modules.
 */
static unsigned long max_loaded_version;

/**
 * Structure defining the compatibility between Objective-C ABI versions.
 */
struct objc_abi_version
{
	/** Version of this ABI. */
	unsigned long version;
	/** Lowest ABI version that this is compatible with. */
	unsigned long min_compatible_version;
	/** Highest ABI version compatible with this. */
	unsigned long max_compatible_version;
	/** Size of the module structure for this ABI version. */
	unsigned long module_size;
};

enum
{
	gcc_abi = 8,
	gnustep_abi = 9,
	gc_abi = 10
};

/**
 * List of supported ABIs.
 */
static struct objc_abi_version known_abis[] =
{
	/* GCC ABI. */
	{gcc_abi, gcc_abi, gnustep_abi, sizeof(struct objc_module_abi_8)},
	/* Non-fragile ABI. */
	{gnustep_abi, gcc_abi, gc_abi, sizeof(struct objc_module_abi_8)},
	/* GC ABI.  Adds a field describing the GC mode. */
	{gc_abi, gcc_abi, gc_abi, sizeof(struct objc_module_abi_10)}
};

static int known_abi_count =
	(sizeof(known_abis) / sizeof(struct objc_abi_version));

#define FAIL_IF(x, msg) do {\
	if (x)\
	{\
		fprintf(stderr, "Objective-C ABI Error: %s\n", msg);\
		return NO;\
	}\
} while(0)

PRIVATE enum objc_gc_mode current_gc_mode = GC_Optional;

PRIVATE BOOL objc_check_abi_version(struct objc_module_abi_8 *module)
{
	unsigned long version = module->version;
	unsigned long module_size = module->size;
	enum objc_gc_mode gc_mode = (version < gc_abi) ? GC_None
	                            : ((struct objc_module_abi_10*)module)->gc_mode;
	struct objc_abi_version *v = NULL;
	for (int i=0 ; i<known_abi_count ; i++)
	{
		if (known_abis[i].version == version)
		{
			v = &known_abis[i];
			break;
		}
	}
	FAIL_IF(NULL == v, "Unknown ABI version");
	FAIL_IF((v->module_size != module_size), "Incorrect module size");
	// Only check for ABI compatibility if 
	if (min_loaded_version > 0)
	{
		FAIL_IF((v->min_compatible_version > min_loaded_version),
				"Loading modules from incompatible ABIs");
		FAIL_IF((v->max_compatible_version < max_loaded_version),
				"Loading modules from incompatible ABIs");
		if (min_loaded_version > version)
		{
			min_loaded_version = version;
		}
		if (max_loaded_version < version)
		{
			max_loaded_version = version;
		}
	}
	else
	{
		min_loaded_version = version;
		max_loaded_version = version;
	}

	// If we're currently in GC-optional mode, then fall to one side or the
	// other if this module requires / doesn't support GC
	if (current_gc_mode == GC_Optional && (gc_mode != current_gc_mode))
	{
		current_gc_mode = gc_mode;
		if (gc_mode != GC_None)
		{
			enableGC(NO);
		}
	}
	// We can't mix GC_None and GC_Required code, but we can mix any other
	// combination
	FAIL_IF((gc_mode != GC_Optional) && (gc_mode != current_gc_mode),
	        "Attempting to mix GC and non-GC code!");
	return YES;
}
