/* src/vm/jit/m68k/emit.h
 
   Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   J. Wenninger, Institut f. Computersprachen - TU Wien

   This file is part of CACAO.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   $Id: arch.h 5330 2006-09-05 18:43:12Z edwin $

*/

#ifndef _EMIT_H_
#define _EMIT_H_

#include "config.h"

#include "vm/types.h"
#include "vm/vm.h"
#include "vm/jit/codegen-common.h"

void emit_mov_imm_reg (codegendata *cd, s4 imm, s4 dreg);
void emit_mov_imm_areg(codegendata *cd, s4 imm, s4 dreg);
void emit_verbosecall_enter(jitdata* jd);
void emit_verbosecall_exit(jitdata* jd);

#endif
