#include "common.S"

#
# This file defines some trampolines for calling blocks.  A block function
# looks like this:
# 
# retType blockFn(block*, ...)
#
# An IMP looks like this:
#
# retType imp(id, SEL,...)
#
# The trampoline must find the block pointer and then call the block function
# with the correct first argument, the self pointer moved to the second real
# argument (the first block argument) and the _cmd parameter excised

.file	"block_trampolines.S"
#ifdef __ARM_ARCH
#ifdef __arm__
.syntax unified
#endif
.globl CDECL(__objc_block_trampoline_sret)
TYPE_DIRECTIVE(CDECL(__objc_block_trampoline_sret), %function)
.globl CDECL(__objc_block_trampoline_end_sret)
.globl CDECL(__objc_block_trampoline)
TYPE_DIRECTIVE(CDECL(__objc_block_trampoline), %function)
.globl CDECL(__objc_block_trampoline_end)
#else
.globl CDECL(__objc_block_trampoline_sret)
TYPE_DIRECTIVE(CDECL(__objc_block_trampoline_sret), @function)
.globl CDECL(__objc_block_trampoline_end_sret)
.globl CDECL(__objc_block_trampoline)
TYPE_DIRECTIVE(CDECL(__objc_block_trampoline), @function)
.globl CDECL(__objc_block_trampoline_end)
#endif
#if __x86_64
#ifdef _WIN64
#define ARG0 %rcx
#define ARG1 %rdx
#define ARG2 %r8
#else
#define ARG0 %rdi
#define ARG1 %rsi
#define ARG2 %rdx
#endif
CDECL(__objc_block_trampoline):
	mov   -0x1007(%rip), ARG1      # Load the block pointer into the second argument
	xchg  ARG1, ARG0           # Swap the first and second arguments
	jmp   *-0x1008(%rip)           # Call the block function
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	mov   -0x1007(%rip), ARG2      # Load the block pointer into the second argument
	xchg  ARG1, ARG2           # Swap the first and second arguments
	jmp   *-0x1008(%rip)           # Call the block function
CDECL(__objc_block_trampoline_end_sret):
#elif __i386
CDECL(__objc_block_trampoline):
	call  Lnext_line            # Store the instruction pointer on the stack
Lnext_line:
	pop   %eax                 # Load the old instruction pointer
	mov   4(%esp), %ebx        # Load the self parameter
	mov   %ebx, 8(%esp)        # Store self as the second argument
	mov   -0x1005(%eax), %ebx       # Load the block pointer to %ebx
	mov   %ebx, 4(%esp)        # Store the block pointer in the first argument
	jmp   *-0x1001(%eax)           # Call the block function
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	call  Lnext_line2           # Store the instruction pointer on the stack
Lnext_line2:
	pop   %eax                 # Load the old instruction pointer
	mov   8(%esp), %ebx        # Load the self parameter
	mov   %ebx, 12(%esp)       # Store self as the second argument
	mov   -0x1005(%eax), %ebx       # Load the block pointer to %ebx
	mov   %ebx, 8(%esp)        # Store the block pointer in the first argument
	jmp   *-0x1001(%eax)           # Call the block function
CDECL(__objc_block_trampoline_end_sret):
#elif __mips__
#	ifdef _ABI64
CDECL(__objc_block_trampoline):
	move     $a1, $a0
	ld       $a0, -4096($25)
	ld       $25, -4088($25)
	jr       $25
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	move     $a2, $a1
	ld       $a1, -4096($25)
	ld       $25, -4088($25)
	jr       $25
CDECL(__objc_block_trampoline_end_sret):
#	else
CDECL(__objc_block_trampoline):
	move   $a1, $a0
	lw     $a0, -4096($25)
	lw     $25, -4092($25)
	jr     $25
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	move   $a2, $a1
	lw     $a1, -4096($25)
	lw     $25, -4092($25)
	jr     $25
CDECL(__objc_block_trampoline_end_sret):
#	endif
#elif defined(__ARM_ARCH_ISA_A64)
.macro trampoline arg0, arg1
	adr x17, #-4096
	mov \arg1, \arg0
	ldp \arg0, x17, [x17]
	br x17
.endm
CDECL(__objc_block_trampoline):
	trampoline x0, x1
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	trampoline x1, x2
CDECL(__objc_block_trampoline_end_sret):
#elif __arm__
#if 0 && ((__ARM_ARCH >= 7) || defined (__ARM_ARCH_6T2__))
// If we're on a target that supports Thumb 2, then we need slightly more
// instructions to support Thumb/ARM code for the IMP and so we need to make
// the trampolines thumb to be able to fit them in 16 bytes (they fit exactly
// when assembled as Thumb-2).
.thumb
.macro trampoline arg0, arg1
	sub r12, pc, #4095
	mov \arg0, \arg1            // Move self over _cmd
	ldr \arg0, [r12, #-5]       // Load the block pointer over self
	ldr r12, [r12, #-1]         // Jump to the block function
	bx  r12
.endm
#else
.macro trampoline arg0, arg1
	sub r12, pc, #4096
	mov \arg0, \arg1            // Move self over _cmd
	ldr \arg0, [r12, #-8]       // Load the block pointer over self
	ldr pc, [r12, #-4]          // Jump to the block function
.endm
#endif // ((__ARM_ARCH >= 7) || defined (__ARM_ARCH_6T2__))
CDECL(__objc_block_trampoline):
	trampoline r0, r1
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	trampoline r1, r2
CDECL(__objc_block_trampoline_end_sret):
#else
#warning imp_implementationWithBlock() not implemented for your architecture
CDECL(__objc_block_trampoline):
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
CDECL(__objc_block_trampoline_end_sret):
#endif
#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
