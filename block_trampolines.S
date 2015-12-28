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
#if __arm__
.syntax unified
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
#elif __arm__
CDECL(__objc_block_trampoline):
	sub r12, pc, #4096
	mov r1, r0                 // Move self over _cmd
	ldr r0, [r12, #-8]         // Load the block pointer over self
	ldr pc, [r12, #-4]         // Jump to the block function
CDECL(__objc_block_trampoline_end):
CDECL(__objc_block_trampoline_sret):
	sub r12, pc, #4096
	mov r2, r1                 // Move self over _cmd
	ldr r1, [r12, #-8]         // Load the block pointer over self
	ldr pc, [r12, #-4]         // Jump to the block function
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
