#define ARGUMENT_SPILL_SIZE (10*8 + 8*8)

.macro MSGSEND receiver, sel
	.cfi_startproc
	beqz     \receiver, 3f                // Skip everything if receiver is nil

	andi     $t0, \receiver, SMALLOBJ_MASK
	bnez     $t0, 5f

	ld.d     $t0, \receiver, 0            // Load class into t0
0:
	ld.d     $t0, $t0, DTABLE_OFFSET      // dtable -> t0
	ld.d     $t1, \sel, 0                 // selector->index -> t1
	ld.w     $t2, $t0, SHIFT_OFFSET       // dtable->shift -> t2

	addi.d   $t3, $zero, 8
	beq      $t2, $t3, 1f
	beqz     $t2, 2f

	srli.d   $t2, $t1, 13                 // Extract byte 3 of sel index and multiply by 2^3
	andi     $t2, $t2, 0x7f8
	add.d    $t2, $t0, $t2
	ld.d     $t0, $t2, DATA_OFFSET
1:
	srli.d   $t2, $t1, 5                  // Extract byte 2 of sel index and multiply by 2^3
	andi     $t2, $t2, 0x7f8
	add.d    $t2, $t0, $t2
	ld.d     $t0, $t2, DATA_OFFSET
2:
	slli.d   $t2, $t1, 3                  // Multiply by 2^3
	andi     $t2, $t2, 0x7f8
	add.d    $t2, $t0, $t2
	ld.d     $t0, $t2, DATA_OFFSET        // Slot pointer is now in t0

	beqz     $t0, 4f                      // If the slot is nil, go to the C path

	ld.d     $t0, $t0, SLOT_OFFSET        // Load the method from the slot
	jr       $t0                          // Tail-call the method

3:
	move     \receiver, $zero
	move     \sel, $zero
	movgr2fr.d $fa0, $zero
	movgr2fr.d $fa1, $zero
	ret

4:
	addi.d   $sp, $sp, -ARGUMENT_SPILL_SIZE

	// Spill function arguments.
	st.d     $a0, $sp, 0
	st.d     $a1, $sp, 8
	st.d     $a2, $sp, 16
	st.d     $a3, $sp, 24
	st.d     $a4, $sp, 32
	st.d     $a5, $sp, 40
	st.d     $a6, $sp, 48
	st.d     $a7, $sp, 56

	// Spill FP arguments.
	fst.d    $fa0, $sp, 64
	fst.d    $fa1, $sp, 72
	fst.d    $fa2, $sp, 80
	fst.d    $fa3, $sp, 88
	fst.d    $fa4, $sp, 96
	fst.d    $fa5, $sp, 104
	fst.d    $fa6, $sp, 112
	fst.d    $fa7, $sp, 120

	st.d     $fp, $sp, 128
	st.d     $ra, $sp, 136

	addi.d   $fp, $sp, 128
	addi.d   $sp, $sp, -16
	st.d     \receiver, $sp, 0            // Keep &self at sp for slowMsgLookup

	// Use explicit DWARF register numbers for compatibility with older Clang IAS.
	.cfi_def_cfa 22, 16
	.cfi_offset 22, -16
	.cfi_offset 1, -8

	move     $a0, $sp                     // &self in first argument
	move     $a1, \sel
	pcaddu18i $ra, %call36(CDECL(slowMsgLookup))
	jirl     $ra, $ra, 0

	move     $t0, $a0                     // IMP -> t0

	ld.d     $a0, $sp, 16
	ld.d     $a1, $sp, 24
	ld.d     $a2, $sp, 32
	ld.d     $a3, $sp, 40
	ld.d     $a4, $sp, 48
	ld.d     $a5, $sp, 56
	ld.d     $a6, $sp, 64
	ld.d     $a7, $sp, 72

	fld.d    $fa0, $sp, 80
	fld.d    $fa1, $sp, 88
	fld.d    $fa2, $sp, 96
	fld.d    $fa3, $sp, 104
	fld.d    $fa4, $sp, 112
	fld.d    $fa5, $sp, 120
	fld.d    $fa6, $sp, 128
	fld.d    $fa7, $sp, 136

	ld.d     $fp, $sp, 144
	ld.d     $ra, $sp, 152
	ld.d     \receiver, $sp, 0

	addi.d   $sp, $sp, ARGUMENT_SPILL_SIZE
	addi.d   $sp, $sp, 16

	jr       $t0                          // Tail-call the method

5:
	pcalau12i $t1, %got_pc_hi20(CDECL(SmallObjectClasses))
	ld.d     $t1, $t1, %got_pc_lo12(CDECL(SmallObjectClasses))
	slli.d   $t0, $t0, 3
	ldx.d    $t0, $t1, $t0
	b        0b
	.cfi_endproc
.endm

.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), %function)
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), %function)
.globl CDECL(objc_msgSend_stret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret), %function)
CDECL(objc_msgSend):
CDECL(objc_msgSend_fpret):
	MSGSEND $a0, $a1
CDECL(objc_msgSend_stret):
	MSGSEND $a1, $a2 // Pointer to stack frame in a0
