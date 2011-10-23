#define DTABLE_OFFSET  64
#define SMALLOBJ_MASK  7
#define SHIFT_OFFSET   4
#define DATA_OFFSET    16
#define SLOT_OFFSET    32
.macro MSGSEND receiver, sel
	test  \receiver, \receiver            # If the receiver is nil
	jz    4f                              # return nil
	movq  $SMALLOBJ_MASK, %r10            # Load the small object mask
	test  \receiver, %r10                 # Check if the receiver is a small object
	jnz   6f                              # Get the small object class

	mov   (\receiver), %r10               # Load the dtable from the class
1:                                        # classLoaded
	mov   DTABLE_OFFSET(%r10), %r10       # Load the dtable from the class
	cmpq  uninstalled_dtable(%rip), %r10  # If this is not (yet) a valid dtable
	je    5f                              # Do a slow lookup
	
	push  %r12
	push  %r13

	mov   (\sel), %r11                    # Load the selector index
	mov   SHIFT_OFFSET(%r10), %r13        # Load the shift (dtable size)
	mov   DATA_OFFSET(%r10), %r12         # load the address of the start of the array
	cmpl  $8, %r13d                       # If this is a small dtable, jump to the small dtable handlers
	je    2f 
	cmpl  $0, %r13d
	je    3f 

	mov   %r11, %r13
	and   $0xff0000, %r13
	shrl  $13, %r13d                      # Right shift 16, but then left shift by 3 *sizeof(void*)
	add   %r13, %r12
	mov   (%r12), %r12
	mov   DATA_OFFSET(%r12), %r12
2:                                        # dtable16:
	mov   %r11, %r13
	and   $0xff00, %r13
	shrl  $5, %r13d
	add   %r13, %r12
	mov   (%r12), %r12
	mov   DATA_OFFSET(%r12), %r12
3:                                       # dtable8:
	mov   %r11, %r13
	and   $0xff, %r13
	shll  $3, %r13d
	add   %r13, %r12
	mov   (%r12), %r10
	pop   %r13
	pop   %r12
	test  %r10, %r10
	jz    5f                             # Nil slot - invoke some kind of forwarding mechanism
	mov   SLOT_OFFSET(%r10), %r10
	jmp   *%r10
4:                                       # returnNil:
	xor   %rax, %rax
	ret
5:                                       # slowSend:
	push  %rax                           # We need to preserve all registers that may contain arguments:
	push  %rbx
	push  %rcx
	push  %rdx
	push  %r8
	push  %r9

	# We're (potentially) modifying the self argument with the lookup, so we don't want to be 
.ifc "\receiver", "%rdi"
	push  %rsi                           # Save _cmd (not preserved across calls)
.else
	push  %rdi                           # Save the sret pointer
	mov   \sel, %rsi                     # move _cmd to where the callee expects it to be
.endif

	push  \receiver                      # Store the receiver on the stack
	mov   %rsp, %rdi                     # First argument is &self
	call  slowMsgLookup                  # Call the slow lookup function
	mov   %rax, %r10                     # Load the returned IMP
	pop   \receiver                      # Load the (potentially) new receiver

.if \receiver == %rdi
	pop   %rsi
.else
	pop   %rdi
.endif

	pop   %r9
	pop   %r8
	pop   %rdx
	pop   %rcx
	pop   %rbx
	pop   %rax
	jmp   *%r10
6:                                        # smallObject:
	and   %rdi, %r10                      # Find the small int type
	shll  $3, %r10d
	lea   SmallObjectClasses(%rip), %r11
	add   %r11, %r10
	mov   (%r10), %r10
	jmp   1b 
.endm
.globl objc_msgSend
	.type	objc_msgSend, @function
objc_msgSend:
	MSGSEND %rdi, %rsi
.globl objc_msgSend_stret
	.type	objc_msgSend_stret, @function
objc_msgSend_stret:
	MSGSEND %rsi, %rdx
