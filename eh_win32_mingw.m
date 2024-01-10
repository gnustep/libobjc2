#include "objc/runtime.h"
#include "objc/objc-exception.h"
#include "objc/hooks.h"
#include <stdio.h>

#include <windows.h>

#define STATUS_GCC_THROW 0x20474343

extern void *__cxa_current_exception_type(void);
extern void __cxa_rethrow();

BOOL handler_installed = NO;
_Thread_local BOOL in_handler = NO;

// This vectored exception handler is the last handler to get invoke for every exception (Objective C or foreign).
// It calls _objc_unexpected_exception only when the exception is a C++ exception (ex->ExceptionCode == STATUS_GCC_THROW)
// and the exception is an Objective C exception.
// It always returns EXCEPTION_CONTINUE_SEARCH, so Windows will continue handling the exception.
static LONG CALLBACK _objc_vectored_exception_handler(EXCEPTION_POINTERS* exceptionInfo)
{
	const EXCEPTION_RECORD* ex = exceptionInfo->ExceptionRecord;

	if (_objc_unexpected_exception != 0
		&& ex->ExceptionCode == STATUS_GCC_THROW
		&& !in_handler)
	{
		// Rethrow the current exception and use the @catch clauses to determine whether it's an Objective C exception
		// or a foreign exception.
		if (__cxa_current_exception_type()) {
			in_handler = YES;
			@try {
				__cxa_rethrow();
			} @catch (id e) {
				// Invoke _objc_unexpected_exception for Objective C exceptions
				(*_objc_unexpected_exception)((id)e);
			} @catch (...) {
				// Ignore foreign exceptions.
			}
			in_handler = NO;
		}
	}

	// EXCEPTION_CONTINUE_SEARCH instructs the exception handler to continue searching for appropriate exception handlers.
	return EXCEPTION_CONTINUE_SEARCH;
}

OBJC_PUBLIC extern objc_uncaught_exception_handler objc_setUncaughtExceptionHandler(objc_uncaught_exception_handler handler)
{
	objc_uncaught_exception_handler previousHandler = __atomic_exchange_n(&_objc_unexpected_exception, handler, __ATOMIC_SEQ_CST);

	// Add a vectored exception handler to support the hook. We only need to do this once.
	if (!handler_installed) {
		AddVectoredExceptionHandler(0 /* The handler is the last handler to be called */ , _objc_vectored_exception_handler);
		handler_installed = YES;
	}

	return previousHandler;
}
