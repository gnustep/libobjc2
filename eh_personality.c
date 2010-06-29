#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "dwarf_eh.h"
#include "objc/runtime.h"
#include "objc/hooks.h"

#include "class.h"

/**
 * Class of exceptions to distinguish between this and other exception types.
 */
#define objc_exception_class (*(int64_t*)"GNUCC++\0")
/**
 * Class used for C++ exceptions.  Used to box them.  
 */
#define cxx_exception_class (*(int64_t*)"GNUCC++\0")

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

/**
 *
 */
void objc_exception_throw(id object)
{
	struct objc_exception *ex = calloc(1, sizeof(struct objc_exception));

	ex->unwindHeader.exception_class = objc_exception_class;

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

	return (Class)objc_getClass(class_name);
}

static BOOL isKindOfClass(struct _Unwind_Exception *ex, Class type)
{
	// Nil is a catchall, but we only want to catch things that are not foreign
	// exceptions in it.
	if (Nil == type)
	{
		return ex->exception_class == objc_exception_class;
	}
	if (ex->exception_class != objc_exception_class)
	{
		// FIXME: Box and stuff.
		return NO;
	}
	id object = *(id*)(ex + 1);

	do
	{
		if (object->isa == type)
		{
			return YES;
		}
		type = class_getSuperclass(type);
	} while (Nil != type);

	return NO;
}


static BOOL check_action_record(struct _Unwind_Context *context,
                                int64_t exceptionClass,
                                struct dwarf_eh_lsda *lsda,
                                dw_eh_ptr_t action_record,
                                struct _Unwind_Exception *ex,
                                unsigned long *selector)
{
	while (action_record)
	{
		int filter = read_sleb128(&action_record);
		dw_eh_ptr_t action_record_offset_base = action_record;
		int displacement = read_sleb128(&action_record);
		*selector = filter;
		if (filter > 0)
		{
			Class type = get_type_table_entry(context, lsda, filter);
			if (ex && isKindOfClass(ex, type))
			{
				return YES;
			}
		}
		else if (filter == 0)
		{
			// Catchall 
			return YES;
		}
		*selector = 0;
		action_record = displacement ? 
			action_record_offset_base + displacement : 0;
	}
	return NO;
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


	// Check if this is a foreign exception.  If it is a C++ exception, then we
	// have to box it.  If it's something else, like a LanguageKit exception
	// then we ignore it (for now)
	BOOL foreignException = exceptionClass != objc_exception_class;

	// If it's not a foreign exception, then we know the layout of the
	// language-specific exception stuff.
	if (!foreignException)
	{
		ex = (struct objc_exception*) ((char*)exceptionObject - 
				offsetof(struct objc_exception, unwindHeader));
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
		BOOL found_handler = check_action_record(context, exceptionClass, &lsda,
				action.action_record, exceptionObject, &selector);
		// If there's no action record, we've only found a cleanup, so keep
		// searching for something real
		if (found_handler)
		{
			// Cache the results for the phase 2 unwind, if we found a handler
			// and this is not a foreign exception.
			if (ex)
			{
				ex->handlerSwitchValue = selector;
				ex->landingPad = action.landing_pad;
			}
			return _URC_HANDLER_FOUND;
		}
		return _URC_CONTINUE_UNWIND;
	}

	if (!(actions & _UA_HANDLER_FRAME) || foreignException)
	{
		struct dwarf_eh_lsda lsda = parse_lsda(context, lsda_addr);
		action = dwarf_eh_find_callsite(context, &lsda);
		if (0 == action.landing_pad) { return _URC_CONTINUE_UNWIND; }
		selector = 0;
	}
	else
	{
		// Restore the saved info if we saved some last time.
		action.landing_pad = ex->landingPad;
		selector = ex->handlerSwitchValue;
	}


	_Unwind_SetIP(context, (unsigned long)action.landing_pad);
	_Unwind_SetGR(context, __builtin_eh_return_data_regno(0), 
			(unsigned long)exceptionObject);
	_Unwind_SetGR(context, __builtin_eh_return_data_regno(1), selector);

	return _URC_INSTALL_CONTEXT;
}
