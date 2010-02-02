
/**
 * Visitor function.  Visiting is non-recursive.  You must call
 * GCVisitChildren() on the object argument if you wish to explore the entire
 * graph.
 */
typedef void (*visit_function_t)(id object, void *context, BOOL isWeak);

void GCVisitChildren(id object, visit_function_t function, void *argument,
		BOOL visitWeakChildren);
