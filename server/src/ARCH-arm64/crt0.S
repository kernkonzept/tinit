/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

	.text

	.globl	_real_start
_real_start:
	ldr 	w9, .LCstack
	adr	x10, _real_start
	add	x9, x9, x10
	mov 	sp, x9

	ldr	w9, .LCkip
	str	x0, [x10, x9]

	b	_start

/* Mind the argc that is popped off the stack! */
.LCstack: .4byte (_stack_top - _real_start - 16)
.LCkip:   .4byte l4_global_kip - _real_start

	.bss
	.p2align 4
_stack_bottom:
	.space 0x2000
_stack_top:
