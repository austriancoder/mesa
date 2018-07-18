/*
 * Copyright (c) 2018 Etnaviv Project
 * Copyright (C) 2018 Zodiac Inflight Innovations
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
#include "util/u_math.h"

static unsigned
instruction_uniform_mask(const struct gc_instr *instr)
{
   const struct gc_instr_alu *alu = &instr->alu;
   unsigned mask = 0;

   for (unsigned i = 0; i < 3; i++) {
      if (alu->src[i].rgroup != GC_REGISTER_GROUP_UNIFORM)
         continue;

      bool is_duplicate = false;
      for (int j = 0; j < i; j++) {
         if (alu->src[j].rgroup == GC_REGISTER_GROUP_UNIFORM &&
             alu->src[j].reg == alu->src[i].reg) {
            is_duplicate = true;
            break;
         }
      }

      if (!is_duplicate)
         mask |= (1U << i);
   }

   return mask;
}

#define for_each_bit(b, dword)                          \
   for (uint32_t __dword = (dword);                     \
        (b) = __builtin_ffs(__dword) - 1, __dword;      \
        __dword &= ~(1 << (b)))

static void
legalize_uniform_usage(struct eir_block *block, struct eir_instruction *instr)
{
   unsigned mask;
   unsigned count;
   unsigned b;
   bool first;

   if (instr->gc.type != GC_OP_TYPE_ALU)
      return;

   mask = instruction_uniform_mask(&instr->gc);
   count = util_bitcount(mask);

   if (count < 2)
      return;

   first = true;
   for_each_bit(b, mask) {
      struct gc_instr_alu *alu = &instr->gc.alu;
      struct gc_instr_src *src = &alu->src[b];
      struct gc_instr_dst dst = {
         .reg = 64
      };

      /* ignore first uniform usage */
      if (first) {
         first = false;
         continue;
      }

      /* generate move instruction to temporary */
      eir_MOV(block, &dst, src);

      /* modify instruction to use temp register instead of uniform */
      src->reg = dst.reg;
      src->rgroup = GC_REGISTER_GROUP_TEMP;
   }
}

static void
legalize_block(struct eir_block *block)
{
   struct list_head instr_list;

   /* remove all the instructions from the list, we'll be adding
    * them back in as we go
    */
   list_replace(&block->instr_list, &instr_list);
   list_inithead(&block->instr_list);

   list_for_each_entry_safe (struct eir_instruction, i, &instr_list, node) {
      legalize_uniform_usage(block, i);
      list_addtail(&i->node, &block->instr_list);
   }

   /* Add nop if block has no instructions. */
   if (list_empty(&block->instr_list))
      eir_NOP(block);
}

void
eir_legalize(struct eir *ir)
{
   list_for_each_entry (struct eir_block, block, &ir->block_list, node)
      legalize_block(block);

   /* resolve branch targets */
   eir_update_ip_data(ir);

   list_for_each_entry (struct eir_block, block, &ir->block_list, node)
      list_for_each_entry (struct eir_instruction, instr, &block->instr_list, node)
         if (instr->gc.opcode == GC_BRANCH)
            instr->gc.branch.imm = block->successors[0]->start_ip;
}
