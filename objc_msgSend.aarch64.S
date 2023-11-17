#define ARGUMENT_SPILL_SIZE (8*10 + 8*16)

#ifdef _WIN64
#	define START_PROC(x) .seh_proc x
#	define END_PROC(x) .seh_endproc
#	define FRAME_OFFSET(x) .seh_stackalloc x
#else
#	define START_PROC(x) .cfi_startproc
#	define END_PROC(x) .cfi_endproc
#	define FRAME_OFFSET(x) .cfi_adjust_cfa_offset x
#endif

.macro MSGSEND fnname, receiver, sel
	START_PROC(\fnname)
	cbz    \receiver, 4f                   // Skip everything if the receiver is nil
	                                       // Jump to 6: if this is a small object
	ubfx    x9, \receiver, #0, #SMALLOBJ_BITS
	cbnz   x9, 6f

	ldr    x9, [\receiver]                 // Load class to x9 if not a small int

.align 2
1:
	ldr    x9, [x9, #DTABLE_OFFSET]        // Dtable -> x9
	ldr    w10, [\sel]                     // selector->index -> x10
	ldr    w11, [x9, #SHIFT_OFFSET]        // dtable->shift -> x11
	
	cmp    x11, #8                         // If this is a small dtable, jump to the
	                                       // small dtable handlers
	b.eq    2f
	cbz    x11, 3f

	ubfx   x11, x10, #16, #8               // Put byte 3 of the sel id in x12
	add    x11, x9, x11, lsl #3            // x11 = dtable address + dtable data offset
	ldr    x9, [x11, #DATA_OFFSET]         // Load, adding in the data offset
2:	                                       // dtable16
	ubfx   x11, x10, #8, #8                // Put byte 2 of the sel id in x12
	add    x11, x9, x11, lsl #3            // x11 = dtable address + dtable data offset
	ldr    x9, [x11, #DATA_OFFSET]         // Load, adding in the data offset
3:                                         // dtable8
	ubfx   x11, x10, #0, #8                // Put low byte of the sel id in x12
	add    x11, x9, x11, lsl #3            // x11 = dtable address + dtable data offset
	ldr    x9, [x11, #DATA_OFFSET]         // Load, adding in the data offset. 
	                                       // Slot pointer is now in x9

	cbz    x9,  5f                         // If the slot is nil, go to the C path

	ldr    x9, [x9, #SLOT_OFFSET]          // Load the method from the slot
	br     x9                              // Tail-call the method

.align 2
4:	                                       // Nil receiver
	mov    x0, #0
	mov    v0.d[0], x0
	mov    v0.d[1], x0
	br     lr
5:                                        // Slow lookup
	                                      // Save anything that will be clobbered by
	                                      // the call.
										  // Note that we pre-index (see "!"), meaning
										  // that we adjust the sp before storing the pair
										  // of registers.
	stp    x0, x1, [sp, #-(ARGUMENT_SPILL_SIZE)]!
	stp    x2, x3, [sp, #16]              // The order is arbitrary, except that
	stp    x4, x5, [sp, #32]              // fp and lr must be spilled together and
	stp    x6, x7, [sp, #48]              // it's convenient if \receiver is spilled at sp
	stp    q0, q1, [sp, #64]
	stp    q2, q3, [sp, #96]
	stp    q4, q5, [sp, #128]
	stp    q6, q7, [sp, #160]
	stp    fp, lr, [sp, #192]
	add    fp, sp, 192					  // Adjust frame pointer
	stp    \receiver, x8, [sp, #-16]!     // Pre-index by sp-16

	// FIXME: Conditionally use SEH directives
	.cfi_def_cfa fp, 16
	.cfi_offset fp, -16
	.cfi_offset lr, -8
	
	                                      // We now have all argument registers, the link
	                                      // register and the receiver spilled on the
	                                      // stack, with sp containing
	                                      // the address of the receiver

	mov    x0, sp                         // &self, _cmd in arguments
	mov    x1, \sel
	bl     CDECL(slowMsgLookup)           // This is the only place where the CFI directives
	                                      // have to be accurate...
	mov    x9, x0                         // IMP -> x9
	ldp    x0, x1, [sp, #16]              // Reload spilled argument registers
	ldp    x2, x3, [sp, #32]
	ldp    x4, x5, [sp, #64]
	ldp    x6, x7, [sp, #64]
	ldp    q0, q1, [sp, #80]
	ldp    q2, q3, [sp, #112]
	ldp    q4, q5, [sp, #144]
	ldp    q6, q7, [sp, #176]
	ldp    fp, lr, [sp, #208]
	ldp    \receiver, x8, [sp], #(ARGUMENT_SPILL_SIZE + 16)
	br     x9
6:
										  // Load 63:12 of SmallObjectClasses address
										  // We use the CDECL macro as Windows prefixes
										  // cdecl conforming symbols with "_".
	adrp   x10, CDECL(SmallObjectClasses) // The macro handles this transparently.

										  // Add lower 12-bits of SmallObjectClasses address to x10
	add    x10, x10, :lo12:CDECL(SmallObjectClasses)
	ldr    x9, [x10, x9, lsl #3]

	b      1b
	END_PROC(\fnname)
.endm

#ifdef _WIN64
.text
.def objc_msgSend;
.scl 2;
.type 32;
.endef
.def objc_msgSend_fpret;
.scl 2;
.type 32;
.endef
.def objc_msgSend_stret;
.scl 2;
.type 32;
.endef
#endif

.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), %function)
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), %function)
.globl CDECL(objc_msgSend_stret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret), %function)
CDECL(objc_msgSend_fpret):
CDECL(objc_msgSend_stret):
CDECL(objc_msgSend):
	MSGSEND objc_msgSend x0, x1

#ifdef _WIN64
.section        .drectve,"yn"
.ascii  " /EXPORT:objc_msgSend"
.ascii  " /EXPORT:objc_msgSend_fpret"
.ascii  " /EXPORT:objc_msgSend_stret"
#endif
