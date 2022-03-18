/**
 * \file
 * \brief	Startup code
 *
 * \date	06/03/2003
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#define __ASSEMBLY__
#include <l4/sys/compiler.h>

/*--- .text (program code) -------------------------*/
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

/*--- my stack -------------------------------------*/
	.bss
	.align 3
_stack_bottom:
	.space 0x1000
_stack_top:
