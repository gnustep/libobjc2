.macro MSGSEND receiver, sel, fpret
	.cfi_startproc                        
	movl  \receiver(%esp), %eax
	test  %eax, %eax                      # If the receiver is nil
	jz    4f                              # return nil
	test  $SMALLOBJ_MASK, %eax            # Check if the receiver is a small object
	jnz   6f                              # Get the small object class

	mov   (%eax), %eax                    # Load the class
1:                                        # classLoaded
	movl  \sel(%esp), %ecx
	mov   DTABLE_OFFSET(%eax), %eax       # Load the dtable from the class
	
	mov   (%ecx), %ecx                    # Load the selector index

	                                      # Register use at this point:
	                                      # %eax: dtable
	                                      # %ecx: Selector index
	                                      # %edx: selector index fragment

	mov   SHIFT_OFFSET(%eax), %edx        # Load the shift (dtable size)
	cmpl  $8, %edx                        # If this is a small dtable, jump to the small dtable handlers
	je    2f 
	cmpl  $0, %edx 
	je    3f 

	mov   %ecx, %edx
	shrl  $16, %edx
	movl  DATA_OFFSET(%eax, %edx, 4), %eax
2:                                        # dtable16:
	movzbl %ch, %edx
	movl  DATA_OFFSET(%eax, %edx, 4), %eax
3:                                        # dtable8:
	movzbl %cl, %edx
	movl  DATA_OFFSET(%eax, %edx, 4), %eax

	test  %eax, %eax
	jz    5f                             # Nil slot - invoke some kind of forwarding mechanism
	mov   SLOT_OFFSET(%eax), %eax
	jmp   *%eax
4:                                       # returnNil:
.if \fpret
	fldz
.else 
	xor   %eax, %eax                     # return 0 (int)
	xor   %edx, %edx                     # Return 64-bit zero (%edx is
	                                     # caller-save, so it's safe to do this in the general case.
.endif
	ret
5:                                       # slowSend:
	mov   \sel(%esp), %ecx
	lea   \receiver(%esp), %eax

	push  %ecx                           # _cmd
	push  %eax                           # &self
	.cfi_def_cfa_offset 12
	call  slowMsgLookup@PLT
	add   $8, %esp                       # restore the stack


	jmp   *%eax
6:                                       # smallObject:
	push  %ebx                           # Save old %ebx
	calll 7f
7:
	popl  %ebx;
8:
	addl  $_GLOBAL_OFFSET_TABLE_+(8b-7b), %ebx
	leal  SmallObjectClasses@GOTOFF(%ebx), %eax
	mov   (%eax), %eax
	popl  %ebx
	jmp   1b 
	.cfi_endproc
.endm
.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), @function)
CDECL(objc_msgSend_fpret):
	MSGSEND 4, 8, 1
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), @function)
CDECL(objc_msgSend):
	MSGSEND 4, 8, 0
.globl CDECL(objc_msgSend_stret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret), @function)
CDECL(objc_msgSend_stret):
	MSGSEND 8, 12, 0
