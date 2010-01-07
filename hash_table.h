/**
 * hash_table.h provides a template for implementing hopscotch hash tables. 
 *
 * Several macros must be defined before including this file:
 *
 * MAP_TABLE_NAME defines the name of the table.  All of the operations and
 * types related to this table will be prefixed with this value.
 *
 * MAP_TABLE_COMPARE_FUNCTION defines the function used for testing a key
 * against a value in the table for equality.  This must take two void*
 * arguments.  The first is the key and the second is the value.  
 *
 * MAP_TABLE_HASH_KEY and MAP_TABLE_HASH_VALUE define a pair of functions that
 * takes a key and a value pointer respectively as their argument and returns
 * an int32_t representing the hash.
 *
 * Optionally, MAP_TABLE_STATIC_SIZE may be defined, to define a table type
 * which has a static size.
 */

#ifndef MAP_TABLE_NAME
#	error You must define MAP_TABLE_NAME.
#endif
#ifndef MAP_TABLE_COMPARE_FUNCTION
#	error You must define MAP_TABLE_COMPARE_FUNCTION.
#endif
#ifndef MAP_TABLE_HASH_KEY 
#	error You must define MAP_TABLE_HASH_KEY
#endif
#ifndef MAP_TABLE_HASH_VALUE
#	error You must define MAP_TABLE_HASH_VALUE
#endif

// Horrible multiple indirection to satisfy the weird precedence rules in cpp
#define REALLY_PREFIX_SUFFIX(x,y) x ## y
#define PREFIX_SUFFIX(x, y) REALLY_PREFIX_SUFFIX(x, y)

/**
 * PREFIX(x) macro adds the table name prefix to the argument.
 */
#define PREFIX(x) PREFIX_SUFFIX(MAP_TABLE_NAME, x)

typedef struct PREFIX(_table_cell_struct)
{
	uint32_t secondMaps;
	void *value;
} *PREFIX(_table_cell);

#ifdef MAP_TABLE_STATIC_SIZE
typedef struct 
{
	unsigned int table_used;
	struct PREFIX(_table_cell_struct) table[MAP_TABLE_STATIC_SIZE];
} PREFIX(_table);
#	define TABLE_SIZE(x) MAP_TABLE_STATIC_SIZE
#else
typedef struct 
{
	unsigned int table_size;
	unsigned int table_used;
	struct PREFIX(_table_cell_struct) table[1];
} PREFIX(_table);
#	define TABLE_SIZE(x) (x->table_size)
#endif

struct PREFIX(_table_enumerator)
{
	unsigned int seen;
	unsigned int index;
};

static inline PREFIX(_table_cell) PREFIX(_table_lookup)(PREFIX(_table) *table, 
                                                        uint32_t hash)
{
	hash = hash % TABLE_SIZE(table);
	return &table->table[hash];
}

static int PREFIX(_table_move_gap)(PREFIX(_table) *table, uint32_t fromHash,
		uint32_t toHash, PREFIX(_table_cell) emptyCell)
{
	for (uint32_t hash = fromHash - 32 ; hash < fromHash ; hash++)
	{
		// Get the cell n before the hash.
		PREFIX(_table_cell) cell = PREFIX(_table_lookup)(table, hash);
		// If this node is a primary entry move it down
		if (MAP_TABLE_HASH_VALUE(cell->value) == hash)
		{
			emptyCell->value = cell->value;
			cell->secondMaps |= (1 << ((fromHash - hash) - 1));
			cell->value = NULL;
			if (hash - toHash < 32)
			{
				return 1;
			}
			return PREFIX(_table_move_gap)(table, hash, toHash, cell);
		}
		int hop = __builtin_ffs(cell->secondMaps);
		if (hop > 0 && (hash + hop) < fromHash)
		{
			PREFIX(_table_cell) hopCell = PREFIX(_table_lookup)(table, hash+hop);
			emptyCell->value = hopCell->value;
			// Update the hop bit for the new offset
			cell->secondMaps |= (1 << ((fromHash - hash) - 1));
			// Clear the hop bit in the original cell
			cell->secondMaps &= ~(1 << (hop - 1));
			hopCell->value = NULL;
			if (hash - toHash < 32)
			{
				return 1;
			}
			return PREFIX(_table_move_gap)(table, hash + hop, toHash, hopCell);
		}
	}
	return 0;
}
static int PREFIX(_table_rebalance)(PREFIX(_table) *table, uint32_t hash)
{
	for (unsigned i=32 ; i<TABLE_SIZE(table) ; i++)
	{
		PREFIX(_table_cell) cell = PREFIX(_table_lookup)(table, hash + i);
		if (NULL == cell->value)
		{
			// We've found a free space, try to move it up.
			return PREFIX(_table_move_gap)(table, hash + i, hash, cell);
		}
	}
	return 0;
}

static int PREFIX(_insert)(PREFIX(_table) *table, 
                                 const void *key, 
                                 void *value)
{
	uint32_t hash = MAP_TABLE_HASH_KEY(key);
	PREFIX(_table_cell) cell = PREFIX(_table_lookup)(table, hash);
	if (NULL == cell->value)
	{
		cell->value = value;
		table->table_used++;
		return 1;
	}
	/* If this cell is full, try the next one. */
	for (unsigned int i=0 ; i<32 ; i++)
	{
		PREFIX(_table_cell) second = 
			PREFIX(_table_lookup)(table, hash+i);
		if (NULL == second->value)
		{
			cell->secondMaps |= (1 << (i-1));
			second->value = value;
			table->table_used++;
			return 1;
		}
	}
	/* If this virtual cell is full, rebalance the hash from this point and
	 * try again. */
	if (PREFIX(_table_rebalance)(table, hash))
	{
		return PREFIX(_insert)(table, key, value);
	}
	return 0;
}

static void *PREFIX(_table_get)(PREFIX(_table) *table, const void *key)
{
	uint32_t hash = MAP_TABLE_HASH_KEY(key);
	PREFIX(_table_cell) cell = PREFIX(_table_lookup)(table, hash);
	// Value does not exist.
	if (NULL == cell->value)
	{
		return NULL;
	}
	if (MAP_TABLE_COMPARE_FUNCTION(key, cell->value))
	{
		return cell->value;
	}
	uint32_t jump = cell->secondMaps;
	// Look at each offset defined by the jump table to find the displaced location.
	for (int hop = __builtin_ffs(jump) ; hop > 0 ; hop = __builtin_ffs(jump))
	{
		PREFIX(_table_cell) hopCell = PREFIX(_table_lookup)(table, hash+hop);
		if (MAP_TABLE_COMPARE_FUNCTION(key, hopCell->value))
		{
			return hopCell->value;
		}
		// Clear the most significant bit and try again.
		jump &= ~(1 << (hop-1));
	}
	return NULL;
}

void *PREFIX(_next)(PREFIX(_table) *table,
                    struct PREFIX(_table_enumerator) **state)
{
	if (NULL == *state)
	{
		*state = calloc(1, sizeof(struct PREFIX(_table_enumerator)));
	}
	if ((*state)->seen >= table->table_used)
	{
		free(*state);
		return NULL;
	}
	while ((++((*state)->index)) < TABLE_SIZE(table))
	{
		if (table->table[(*state)->index].value != NULL)
		{
			return table->table[(*state)->index].value;
		}
	}
	// Should not be reached, but may be if the table is unsafely modified.
	free(*state);
	return NULL;
}

#undef TABLE_SIZE
#undef REALLY_PREFIX_SUFFIX
#undef PREFIX_SUFFIX
#undef PREFIX

#undef MAP_TABLE_NAME
#undef MAP_TABLE_COMPARE_FUNCTION
#undef MAP_TABLE_HASH_KEY
#undef MAP_TABLE_HASH_VALUE
#ifdef MAP_TABLE_STATIC_SIZE
#	undef MAP_TABLE_STATIC_SIZE
#endif
