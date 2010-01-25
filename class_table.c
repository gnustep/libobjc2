#include "objc/objc-api.h"
#include "lock.h"
#include <stdlib.h>

// Get the functions for string hashing
#include "string_hash.h"

static int class_compare(const char *name, const Class class)
{
	return string_compare(name, class->name);
}
static int class_hash(const Class class)
{
	return string_hash(class->name);
}
#define MAP_TABLE_NAME class_table_internal
#define MAP_TABLE_COMPARE_FUNCTION class_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE class_hash
// This defines the maximum number of classes that the runtime supports.
/*
#define MAP_TABLE_STATIC_SIZE 2048
#define MAP_TABLE_STATIC_NAME class_table
*/
#include "hash_table.h"

static class_table_internal_table *class_table;

void class_table_insert(Class class)
{
	class_table_internal_insert(class_table, class);
}

Class class_table_get_safe(const char *class_name)
{
	return class_table_internal_table_get(class_table, class_name);
}

Class
class_table_next (void **e)
{
	return class_table_internal_next(class_table, 
			(struct class_table_internal_table_enumerator**)e);
}

void __objc_init_class_tables(void)
{
	class_table = class_table_internal_create(16);
}
