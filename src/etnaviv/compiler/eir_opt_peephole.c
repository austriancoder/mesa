/*
 * Copyright (c) 2019 Etnaviv Project
 * Copyright (C) 2019 Zodiac Inflight Innovations
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "eir.h"
#include "eir_optimize.h"

static bool
eir_opt_peephole_impl(struct eir_instruction *instr)
{
   /* remove "mov t0 t0" as seen with bin/gl-2.0-vertex-attr-0 */

   if (instr->gc.opcode != GC_MOV)
      return false;

   if (instr->src[0].index != instr->dst.index)
      return false;

   if (instr->dst.writemask != (INST_COMPS_X | INST_COMPS_Y | INST_COMPS_Z | INST_COMPS_W))
      return false;

   if (instr->src[0].swizzle != INST_SWIZ_IDENTITY)
      return false;

   list_del(&instr->node);

   return true;
}

bool
eir_opt_peephole(struct eir *ir)
{
   bool progress = false;

   eir_for_each_block(block, ir)
      eir_for_each_inst_safe(inst, block)
         progress |= eir_opt_peephole_impl(inst);

   return progress;
}
