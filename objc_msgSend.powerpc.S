// r1 (sp), r31, lr (link register), r3-r10 (parameters), f0-f13
#define ARGUMENT_SPILL_SIZE (3*8 + 8*8 + 14*8)

// We use r0, r11, and r12 as temporary registers
// word = 4 bytes
// doubleword = 8 bytes
.macro MSGSEND receiver, sel
	.cfi_startproc

    cmpldi \receiver, 0
    beq 3f                               // Skip everything if receiver is nil

    andi.  %r0, \receiver, SMALLOBJ_MASK // CR0[EQ] = 1 if this is not a small obj
    bne 5f                               // Branch to 5f if CR0[EQ] = 0 -> small obj

    ld %r0, 0(\receiver)                 // Load class into r0
0:
    ld %r0, DTABLE_OFFSET(%r0)           // dtable -> r0
    ld %r11, 0(\sel)                     // selector->index -> r11
    ld %r12, SHIFT_OFFSET(%r0)           // dtable->shift -> r12

    cmpldi %r12, 8
    beq 1f
    cmpldi %r12, 0
    beq 2f

    srdi %r12, %r11, 16-3                // Extract byte 3 of sel index and multiply by 2^3
    andi. %r12, %r12, 0x7F8	 	         // Mask target byte
				             			 // Example: ((0xCAFEBA >> 13) & 0x7f8) == (0xCA << 3)
    add %r12, %r0, %r12                  // r12 = dtable address + offset
    ld %r0, DATA_OFFSET(%r12)            // Load, adding in the data offset
1:
    srdi %r12, %r11, 8-3                 // Extract byte 2 of sel index and multiply by 2^3
    andi. %r12, %r12, 0x7F8	 	         // Mask target byte
    add %r12, %r0, %r12                  // r12 = dtable address + offset
    ld %r0, DATA_OFFSET(%r12)            // Load, adding in the data offset
2:
    sldi %r12, %r11, 3                   // Multiply by 2^3
    andi. %r12, %r12, 0x7F8	 	         // Mask target byte
    add %r12, %r0, %r12                  // r12 = dtable address + offset
    ld %r0, DATA_OFFSET(%r12)            // Load, adding in the data offset
                                         // Slot pointer is now in t0
 
    cmpldi %r0, 0
    beq 4f                               // If the slot is nil, go to the C path

    ld %r0, SLOT_OFFSET(%r0)             // Load the method from the slot

    mtctr %r0
    bctr                                 // Tail-call the method
3:
    li \receiver, 0
    li \sel, 0

    addi %r1, %r1, -8                    // Allocate 8 bytes of stack space
    std \receiver, 0(%r1)                // zero'd dword at r1

    lfd %f1, 0(%r1)                      // Zero all fp return registers
    lfd %f2, 0(%r1)                      // Clang uses f1-f8 for struct ret
    lfd %f3, 0(%r1)                      // NOTE: Stack must be dword aligned!
    lfd %f4, 0(%r1)
    lfd %f5, 0(%r1)
    lfd %f6, 0(%r1)
    lfd %f7, 0(%r1)
    lfd %f8, 0(%r1)

    addi %r1, %r1, 8                     // Deallocate stack space

    blr                                  // Return to caller
4:
    stdu %r1, -(ARGUMENT_SPILL_SIZE)(%r1)
    .cfi_def_cfa_offset ARGUMENT_SPILL_SIZE
    
    // Spill function arguments (r3-r10)
    std %r3,  8(%r1)
    std %r4,  16(%r1)
    std %r5,  24(%r1)
    std %r6,  32(%r1)
    std %r7,  40(%r1)
    std %r8,  48(%r1)
    std %r9,  56(%r1)
    std %r10, 64(%r1)

    // Spill FP arguments and scratch register (f0-f13)
    stfd %f0,  72(%r1)
    stfd %f1,  80(%r1)
    stfd %f2,  88(%r1)
    stfd %f3,  96(%r1)
    stfd %f4,  104(%r1)
    stfd %f5,  112(%r1)
    stfd %f6,  120(%r1)
    stfd %f7,  128(%r1)
    stfd %f8,  136(%r1)
    stfd %f9,  144(%r1)
    stfd %f10, 152(%r1)
    stfd %f11, 160(%r1)
    stfd %f12, 168(%r1)
    stfd %f13, 176(%r1)
    
    mflr %r0
    std  %r31, 184(%r1)
    std  %r0,  192(%r1)
    
    .cfi_offset 31, -184  // Mark location of r31
    .cfi_offset 65, -192  // Mark location of lr

    addi %r1, %r1, -8
    .cfi_adjust_cfa_offset -8

    std \receiver, 0(%r1) // it is convenient if \receiver is spilled at r1 (sp)

    mr %r3, %r1           // &self
    bl CDECL(slowMsgLookup)
    mr %r11, %r3          // IMP -> r11

    // Reload spilled registers
    ld \receiver, 0(%r1)
    addi %r1, %r1, 8
    .cfi_adjust_cfa_offset 8

    ld %r3,  8(%r1)
    ld %r4,  16(%r1)
    ld %r5,  24(%r1)
    ld %r6,  32(%r1)
    ld %r7,  40(%r1)
    ld %r8,  48(%r1)
    ld %r9,  56(%r1)
    ld %r10, 64(%r1)

    lfd %f0,  72(%r1)
    lfd %f1,  80(%r1)
    lfd %f2,  88(%r1)
    lfd %f3,  96(%r1)
    lfd %f4,  104(%r1)
    lfd %f5,  112(%r1)
    lfd %f6,  120(%r1)
    lfd %f7,  128(%r1)
    lfd %f8,  136(%r1)
    lfd %f9,  144(%r1)
    lfd %f10, 152(%r1)
    lfd %f11, 160(%r1)
    lfd %f12, 168(%r1)
    lfd %f13, 176(%r1)
    
    ld  %r31, 184(%r1)
    ld  %r0,  192(%r1)
    mtlr %r0        // Restore lr

    addi %r1, %r1, ARGUMENT_SPILL_SIZE
    .cfi_adjust_cfa_offset -ARGUMENT_SPILL_SIZE

    mtctr %r11
    bctr                                 // Tail-call the method
5:
    // Load address of SmallObjectClasses
    addis %r11, %r2, CDECL(SmallObjectClasses)@toc@ha
    addi %r11, %r11, CDECL(SmallObjectClasses)@toc@l

    sldi %r0, %r0, 3 // Multiply by 2^3
    add %r0, %r11, %r0

    ld %r0, 0(%r0)

    b 0b
	.cfi_endproc
.endm

.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), %function)
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), %function)
.globl CDECL(objc_msgSend_stret)
CDECL(objc_msgSend):
CDECL(objc_msgSend_fpret):
	MSGSEND %r3, %r4
CDECL(objc_msgSend_stret):
	MSGSEND %r4, %r5 // Pointer to stack frame in %r3
