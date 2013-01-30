#ifndef _OBJC_MESSAGE_H_
#define _OBJC_MESSAGE_H_

#if defined(__x86_64) || defined(__i386) || defined(__arm__) || \
	defined(__mips_n64) || defined(__mips_n32)
/**
 * Standard message sending function.  This function must be cast to the
 * correct types for the function before use.  The first argument is the
 * receiver and the second the selector.
 *
 * Note that this function is not available on all architectures.  For a more
 * portable solution to sending arbitrary messages, consider using
 * objc_msg_lookup_sender() and then calling the returned IMP directly.
 *
 * This version of the function is used for all messages that return either an
 * integer, a pointer, or a small structure value that is returned in
 * registers.  Be aware that calling conventions differ between operating
 * systems even within the same architecture, so take great care if using this
 * function for small (two integer) structures.
 */
id objc_msgSend(id self, SEL _cmd, ...);
/**
 * Standard message sending function.  This function must be cast to the
 * correct types for the function before use.  The first argument is the
 * receiver and the second the selector.
 *
 * Note that this function is not available on all architectures.  For a more
 * portable solution to sending arbitrary messages, consider using
 * objc_msg_lookup_sender() and then calling the returned IMP directly.
 *
 * This version of the function is used for all messages that return a
 * structure that is not returned in registers.  Be aware that calling
 * conventions differ between operating systems even within the same
 * architecture, so take great care if using this function for small (two
 * integer) structures.
 */
#ifdef __cplusplus 
id objc_msgSend_stret(id self, SEL _cmd, ...);
#else
// There is a bug in older versions of clang that incorrectly declares the
// signature of this function as a builtin.
#	ifdef __clang__
#		if (__clang_major__ > 3) || ((__clang_major__ == 3) && __clang_minor__ >= 3)
void objc_msgSend_stret(id self, SEL _cmd, ...);
#		else
id objc_msgSend_stret(id self, SEL _cmd, ...);
#		endif
#	else 
void objc_msgSend_stret(id self, SEL _cmd, ...);
#	endif
#endif
/**
 * Standard message sending function.  This function must be cast to the
 * correct types for the function before use.  The first argument is the
 * receiver and the second the selector.
 *
 * Note that this function is not available on all architectures.  For a more
 * portable solution to sending arbitrary messages, consider using
 * objc_msg_lookup_sender() and then calling the returned IMP directly.
 *
 * This version of the function is used for all messages that return floating
 * point values.
 */
long double objc_msgSend_fpret(id self, SEL _cmd, ...);

#endif

#endif //_OBJC_MESSAGE_H_
