#include "objc/runtime.h"
#include "module.h"
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

/**
 * List of supported ABIs.
 */
static struct objc_abi_version known_abis[] =
{
	/* GCC ABI. */
	{8, 8, 9, sizeof(struct objc_module_abi_8)},
	/* Clang ABI. */
	{9, 8, 9, sizeof(struct objc_module_abi_8)}
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

BOOL objc_check_abi_version(unsigned long version, unsigned long module_size)
{
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
	return YES;
}
