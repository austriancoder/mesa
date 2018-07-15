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

static uint32_t
instruction_uniform_count(const struct gc_instr *instr)
{
   const struct gc_instr_alu *alu = &instr->alu;
   uint32_t count = 0;

   for (int i = 0; i < 3; i++) {
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
         count++;
   }
   return count;
}

#include <stdio.h>

bool
eir_legalize(struct eir *ir)
{
   bool legalized = false;

   /* Walk the instruction list, finding which instructions have more
    * than one uniform referenced.
    */
   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (const struct eir_instruction, instr, &block->instr_list, node) {

         if (instr->gc.type != GC_OP_TYPE_ALU)
            continue;

         if (instruction_uniform_count(&instr->gc) < 2)
            continue;

         /* TODO: fix up instruction by adding an extra mov */
         printf("@@!! too many uniforms per instr!\n");
         legalized = true;
      }
   }

   /* Add nop if block has no instructions. */
   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      if (list_empty(&block->instr_list)) {
         eir_NOP(block);
         legalized = true;
      }
   }

   /* resolve branch targets */
   eir_update_ip_data(ir);

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (struct eir_instruction, instr, &block->instr_list, node) {
         if (instr->gc.opcode == GC_BRANCH) {
            instr->gc.branch.imm = block->successors[0]->start_ip;
            legalized = true;
         }
      }
   }

   return legalized;
}
