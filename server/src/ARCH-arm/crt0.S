/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

	.text

	.globl	_real_start
_real_start:
	ldr 	sp, .LCstack
	ldr	r4, .LCkip
	adr	r12, _real_start
	add	sp, sp, r12
	str	r0, [r12, r4]
	b	_start

/* Mind the argc that is popped off the stack! */
.LCstack: .word (_stack_top - _real_start - 8)
.LCkip:   .word l4_global_kip - _real_start

	.bss
	.p2align 3
_stack_bottom:
	.space 0x1000
_stack_top:
