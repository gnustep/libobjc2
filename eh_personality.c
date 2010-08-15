#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "dwarf_eh.h"
#include "objc/runtime.h"
#include "objc/hooks.h"

#include "class.h"
#define fprintf(...)

/**
 * Class of exceptions to distinguish between this and other exception types.
 */
#define objc_exception_class (*(int64_t*)"GNUCOBJC")

/**
 * Type info used for catch handlers that catch id.
 */
const char *__objc_id_typeinfo = "id";

/**
 * Structure used as a header on thrown exceptions.  
 */
struct objc_exception
{
	/** The selector value to be returned when installing the catch handler.
	 * Used at the call site to determine which catch() block should execute.
	 * This is found in phase 1 of unwinding then installed in phase 2.*/
	int handlerSwitchValue;
	/** The cached landing pad for the catch handler.*/
	void *landingPad;

	/** The language-agnostic part of the exception header. */
	struct _Unwind_Exception unwindHeader;
	/** Thrown object.  This is after the unwind header so that the C++
	 * exception handler can catch this as a foreign exception. */
	id object;
};

typedef enum
{
	handler_none,
	handler_cleanup,
	handler_catchall,
	handler_class
} handler_type;

static void cleanup(_Unwind_Reason_Code reason, struct _Unwind_Exception *e)
{
	free((struct objc_exception*) ((char*)e - offsetof(struct objc_exception,
					unwindHeader)));
}
/**
 *
 */
void objc_exception_throw(id object)
{
	struct objc_exception *ex = calloc(1, sizeof(struct objc_exception));

	ex->unwindHeader.exception_class = objc_exception_class;
	ex->unwindHeader.exception_cleanup = cleanup;

	ex->object = object;

	_Unwind_Reason_Code err = _Unwind_RaiseException(&ex->unwindHeader);
	free(ex);
	if (_URC_END_OF_STACK == err && 0 != _objc_unexpected_exception)
	{
		_objc_unexpected_exception(object);
	}
	abort();
}

Class get_type_table_entry(struct _Unwind_Context *context,
                           struct dwarf_eh_lsda *lsda,
                           int filter)
{
	dw_eh_ptr_t record = lsda->type_table -
		dwarf_size_of_fixed_size_field(lsda->type_table_encoding)*filter;
	dw_eh_ptr_t start = record;
	int64_t offset = read_value(lsda->type_table_encoding, &record);

	if (0 == offset) { return Nil; }

	// ...so we need to resolve it
	char *class_name = (char*)(intptr_t)resolve_indirect_value(context,
			lsda->type_table_encoding, offset, start);

	if (0 == class_name) { return Nil; }

	fprintf(stderr, "Class name: %d\n", class_name);

	if (__objc_id_typeinfo == class_name) { return (Class)1; }

	return (Class)objc_getClass(class_name);
}

static BOOL isKindOfClass(Class thrown, Class type)
{
	do
	{
		if (thrown == type)
		{
			return YES;
		}
		thrown = class_getSuperclass(thrown);
	} while (Nil != thrown);

	return NO;
}


static handler_type check_action_record(struct _Unwind_Context *context,
                                        BOOL foreignException,
                                        struct dwarf_eh_lsda *lsda,
                                        dw_eh_ptr_t action_record,
                                        Class thrown_class,
                                        unsigned long *selector)
{
	//if (!action_record) { return handler_cleanup; }
	while (action_record)
	{
		int filter = read_sleb128(&action_record);
		dw_eh_ptr_t action_record_offset_base = action_record;
		int displacement = read_sleb128(&action_record);
		*selector = filter;
		fprintf(stderr, "Filter: %d\n", filter);
		if (filter > 0)
		{
			Class type = get_type_table_entry(context, lsda, filter);
			fprintf(stderr, "%p type: %d\n", type, !foreignException);
			// Catchall
			if (Nil == type)
			{
				return handler_catchall;
			}
			// We treat id catches as catchalls when an object is thrown and as
			// nothing when a foreign exception is thrown
			else if ((Class)1 == type)
			{
				if (!foreignException)
				{
					return handler_catchall;
				}
			}
			else if (!foreignException && isKindOfClass(thrown_class, type))
			{
				fprintf(stderr, "found handler for %s\n", type->name);
				return handler_class;
			}
		}
		else if (filter == 0)
		{
			fprintf(stderr, "0 filter\n");
			// Cleanup?  I think the GNU ABI doesn't actually use this, but it
			// would be a good way of indicating a non-id catchall...
			return handler_cleanup;
		}
		else
		{
			fprintf(stderr, "Filter value: %d\n"
					"Your compiler and I disagree on the correct layout of EH data.\n", 
					filter);
			abort();
		}
		*selector = 0;
		action_record = displacement ? 
			action_record_offset_base + displacement : 0;
	}
	return handler_none;
}

/**
 * The exception personality function.  
 */
_Unwind_Reason_Code  __gnu_objc_personality_v0(int version,
                                               _Unwind_Action actions,
                                               uint64_t exceptionClass,
                                               struct _Unwind_Exception *exceptionObject,
                                               struct _Unwind_Context *context)
{
	// This personality function is for version 1 of the ABI.  If you use it
	// with a future version of the ABI, it won't know what to do, so it
	// reports a fatal error and give up before it breaks anything.
	if (1 != version)
	{
		return _URC_FATAL_PHASE1_ERROR;
	}
	struct objc_exception *ex = 0;

	//char *cls = (char*)&exceptionClass;
	fprintf(stderr, "Class: %c%c%c%c%c%c%c%c\n", cls[0], cls[1], cls[2], cls[3], cls[4], cls[5], cls[6], cls[7]);

	// Check if this is a foreign exception.  If it is a C++ exception, then we
	// have to box it.  If it's something else, like a LanguageKit exception
	// then we ignore it (for now)
	BOOL foreignException = exceptionClass != objc_exception_class;

	Class thrown_class = Nil;

	// If it's not a foreign exception, then we know the layout of the
	// language-specific exception stuff.
	if (!foreignException)
	{
		ex = (struct objc_exception*) ((char*)exceptionObject - 
				offsetof(struct objc_exception, unwindHeader));

		thrown_class = ex->object->isa;
	}
	else if (_objc_class_for_boxing_foreign_exception)
	{
		thrown_class = _objc_class_for_boxing_foreign_exception(exceptionClass);
		fprintf(stderr, "Foreign class: %p\n", thrown_class);
	}
	unsigned char *lsda_addr = (void*)_Unwind_GetLanguageSpecificData(context);

	// No LSDA implies no landing pads - try the next frame
	if (0 == lsda_addr) { return _URC_CONTINUE_UNWIND; }

	// These two variables define how the exception will be handled.
	struct dwarf_eh_action action = {0};
	unsigned long selector = 0;
	
	if (actions & _UA_SEARCH_PHASE)
	{
		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
		action = dwarf_eh_find_callsite(context, &lsda);
		handler_type handler = check_action_record(context, foreignException,
				&lsda, action.action_record, thrown_class, &selector);
		// If there's no action record, we've only found a cleanup, so keep
		// searching for something real
		if (handler == handler_class ||
		    ((handler == handler_catchall)))// && !foreignException))
		{
			// Cache the results for the phase 2 unwind, if we found a handler
			// and this is not a foreign exception.
			if (ex)
			{
				ex->handlerSwitchValue = selector;
				ex->landingPad = action.landing_pad;
			}
			fprintf(stderr, "Found handler! %d\n", handler);
			return _URC_HANDLER_FOUND;
		}
		return _URC_CONTINUE_UNWIND;
	}
	fprintf(stderr, "Phase 2: Fight!\n");

	// TODO: If this is a C++ exception, we can cache the lookup and cheat a
	// bit
	void *object = nil;
	if (!(actions & _UA_HANDLER_FRAME))
	{
		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
		action = dwarf_eh_find_callsite(context, &lsda);
		// If there's no cleanup here, continue unwinding.
		if (0 == action.landing_pad)
		{
			return _URC_CONTINUE_UNWIND;
		}
		handler_type handler = check_action_record(context, foreignException,
				&lsda, action.action_record, thrown_class, &selector);
		fprintf(stderr, "handler! %d %d\n",handler, selector);
		// If this is not a cleanup, ignore it and keep unwinding.
		//if (check_action_record(context, foreignException, &lsda,
				//action.action_record, thrown_class, &selector) != handler_cleanup)
		if (handler != handler_cleanup)
		{
			fprintf(stderr, "Ignoring handler! %d\n",handler);
			return _URC_CONTINUE_UNWIND;
		}
		fprintf(stderr, "Installing cleanup...\n");
		// If there is a cleanup, we need to return the exception structure
		// (not the object) to the calling frame.  The exception object
		object = exceptionObject;
		//selector = 0;
	}
	else if (foreignException)
	{
		fprintf(stderr, "Doing the foreign exception thing...\n");
		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
		action = dwarf_eh_find_callsite(context, &lsda);
		check_action_record(context, foreignException, &lsda,
				action.action_record, thrown_class, &selector);
		//[thrown_class exceptionWithForeignException: exceptionObject];
		SEL box_sel = sel_registerName("exceptionWithForeignException:");
		IMP boxfunction = objc_msg_lookup((id)thrown_class, box_sel);
		object = boxfunction((id)thrown_class, box_sel, exceptionObject);
	}
	else
	{
		// Restore the saved info if we saved some last time.
		action.landing_pad = ex->landingPad;
		selector = ex->handlerSwitchValue;
		object = ex->object;
		free(ex);
	}

	_Unwind_SetIP(context, (unsigned long)action.landing_pad);
	_Unwind_SetGR(context, __builtin_eh_return_data_regno(0), 
			(unsigned long)object);
	_Unwind_SetGR(context, __builtin_eh_return_data_regno(1), selector);

	return _URC_INSTALL_CONTEXT;
}
