.syntax unified
.fpu neon
#if ((__ARM_ARCH >= 7) || defined (__ARM_ARCH_6T2__))
.macro byte1 dst, src
	uxtb   \dst, \src
.endm
.macro byte2 dst, src
	ubfx   \dst, \src, #8, #8
.endm
.macro byte3 dst, src
	ubfx   \dst, \src, #16, #8
.endm
#else
.macro byte1 dst, src
	and \dst, \src, #0xff
.endm
.macro byte2 dst, src
	and \dst, \src, #0xff00
	lsr \dst, \dst, 8
.endm
.macro byte3 dst, src
	and \dst, \src, #0xff00
	lsr \dst, \dst, 16
.endm
#endif

// Macro for testing: logs a register value to standard error
.macro LOG reg
	push {r0-r3, ip,lr}
	mov r0, \reg
	bl  logInt(PLT)
	pop {r0-r3, ip,lr}
.endm

.macro MSGSEND receiver, sel
	.fnstart
	teq    \receiver, 0
	beq    4f                              // Skip everything if the receiver is nil
	push   {r4-r6}                         // We're going to use these three as
	.save  {r4-r6}
	                                       // scratch registers, so save them now.
	                                       // These are callee-save, so the unwind library
	                                       // must be able to restore them, so we need CFI
	                                       // directives for them, but not for any other pushes
	tst    \receiver, SMALLOBJ_MASK        // Sets Z if this is not a small int


	ldrne  r4, LSmallIntClass              // Small Int class -> r4 if this is a small int
	ldrne  r4, [r4]

	ldreq  r4, [\receiver]                 // Load class to r4 if not a small int

	ldr    r4, [r4, #DTABLE_OFFSET]        // Dtable -> r4

	ldr    r5, [\sel]                      // selector->index -> r5

	ldr    r6, [r4, #SHIFT_OFFSET]        // dtable->shift -> r6
	
	teq    r6, #8                         // If this is a small dtable, jump to the small dtable handlers
	beq    1f
	teq    r6, #0
	beq    2f

	byte3  r6, r5                         // Put byte 3 of the sel id in r6
	add    r6, r4, r6, lsl #2             // r6 = dtable address + dtable data offset
	ldr    r4, [r6, #DATA_OFFSET]         // Load, adding in the data offset
1:                                        // dtable16
	byte2  r6, r5                         // Put byte 2 of the sel id in r6
	add    r6, r4, r6, lsl #2             // r6 = dtable address + dtable data offset
	ldr    r4, [r6, #DATA_OFFSET]         // Load, adding in the data offset
2:                                        // dtable8
	byte1  r6, r5                         // Low byte of sel id into r5
	add    r6, r4, r6, lsl #2             // r6 = dtable address + dtable data offset
	ldr    ip, [r6, #DATA_OFFSET]         // Load, adding in the data offset

	teq    ip, #0                         // If the slot is nil
	beq    5f                             // Go to the slow path and do the forwarding stuff

	ldr   ip, [ip, #SLOT_OFFSET]          // Load the method from the slot

3: 
	pop    {r4-r6}                        // Restore the saved callee-save registers
	mov    pc, ip

4:                                        // Nil receiver
	mov    r0, 0
	mov    r1, 0
	mov    pc, lr
5:                                        // Slow lookup
	push   {r0-r4, lr}                    // Save anything that will be clobbered by the call
	.save  {r0-r4, lr}


	push   {\receiver}                    // &self, _cmd in arguments
	.save  {\receiver}
#ifndef __SOFTFP__
	vpush  {q0-q3}
	.pad   #64
#endif


	mov    r0, sp
	mov    r1, \sel

	bl     CDECL(slowMsgLookup)(PLT)      // This is the only place where the CFI directives have to be accurate...
	mov    ip, r0                         // IMP -> ip

	pop    {r5}                           // restore (modified) self to r5
#ifndef __SOFTFP__
	vpop   {q0-q3}
#endif
	pop    {r0-r4, lr}                    // Load clobbered registers
	mov    \receiver, r5
	b      3b
	.fnend
.endm

.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), %function)
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), %function)
CDECL(objc_msgSend):
CDECL(objc_msgSend_fpret):
	MSGSEND r0, r1
.globl CDECL(objc_msgSend_stret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret), %function)
CDECL(objc_msgSend_stret):
	MSGSEND r1, r2

LSmallIntClass:
	.long   SmallObjectClasses
	.align  2
