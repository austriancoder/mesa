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
#include "vivante/gc/gc_instr.h"

static int
invalid_uniform_usage(const struct eir_instruction *inst)
{
   const struct gc_instr *gc = &inst->gc;
   int invalid = 0;
   bool first_uniform = true;
   int index;

   if (gc->type != GC_OP_TYPE_ALU)
      return 0;

   for (unsigned i = 0; i < gc_op_num_src(gc->opcode); i++) {
      const struct eir_register *src = &inst->src[i];

      if (src->type != EIR_REG_UNIFORM)
         continue;

      if (first_uniform) {
         index = src->index;
         first_uniform = false;
         continue;
      }

      if (src->index == index)
         continue;

      invalid |= 1 << i;
   }

   return invalid;
}

static void
legalize_uniform_usage(struct eir_block *block, struct eir_instruction *inst)
{
   /*
   * The hardware does not allow two or more different uniform registers to be used as
   * sources in the same ALU instruction. Emit mov instructions to temporary registers
   * for all but one uniform register in this case.
   */
   int mask = invalid_uniform_usage(inst);

   while (mask) {
      const int i = ffs(mask) - 1;
      struct eir_register *src = &inst->src[i];
      struct eir_register tmp = eir_temp_register(block->ir, 4);

      tmp.writemask = 0xf; /* TODO */

      eir_MOV(block, &tmp, src);
      src->type = EIR_REG_TEMP;
      src->index = tmp.index;

      mask &= ~(1 << i);
   }
}

static void
legalize_block(struct eir_block *block)
{
   struct list_head instr_list;

   /*
    * Remove all the instructions from the list, we'll be adding
    * them back in as we go
    */
   list_replace(&block->instr_list, &instr_list);
   list_inithead(&block->instr_list);

   list_for_each_entry_safe (struct eir_instruction, inst, &instr_list, node) {
      legalize_uniform_usage(block, inst);
      list_addtail(&inst->node, &block->instr_list);
   }
}

struct block_data {
   unsigned start_ip;
   unsigned end_ip;
};

static void
resolve_jumps(struct eir *ir)
{
   void *mem_ctx = ralloc_context(NULL);
   unsigned ip = 0;
   assert(mem_ctx);

   eir_for_each_block(block, ir) {
      struct block_data *bd = rzalloc(mem_ctx, struct block_data);

      assert(bd);
      assert(!block->data);
      block->data = bd;

      /* determine start and end IP for this block */
      bd->start_ip = ip;
      eir_for_each_inst(inst, block) {
         ip++;
      }
      bd->end_ip = ip;
   }

   eir_for_each_block(block, ir) {
      const struct block_data *bd;

      /* the end block of the program has no branch */
      if (!block->successors[0])
         continue;

      bd = block->successors[0]->data;

      eir_for_each_inst(inst, block) {
         if (inst->gc.type != GC_OP_TYPE_BRANCH)
            continue;

         inst->gc.branch.imm = bd->start_ip;

         /*
          * if there is a empty block at the end of the shader an
          * extra NOP should be generated as jump target
          */
         if (list_empty(&block->successors[0]->instr_list))
            eir_NOP(block->successors[0]);
      }
   }

   ralloc_free(mem_ctx);
   eir_for_each_block(block, ir)
      block->data = NULL;
}

void
eir_legalize(struct eir *ir)
{
   eir_for_each_block(block, ir)
      legalize_block(block);

   resolve_jumps(ir);

   /* add NOP if the only block has no instructions */
   if (ir->blocks == 1) {
      struct eir_block *block = list_first_entry(&ir->block_list, struct eir_block, node);

      if (list_empty(&block->instr_list))
         eir_NOP(block);
   }
}
