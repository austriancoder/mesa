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
#include "util/ralloc.h"

static inline void
setup_use(struct eir_block *block, const struct gc_instr_src *src, int ip)
{
   const int var = src->reg;
   const struct eir *ir = block->ir;

   if (!src->use)
      return;

   if (src->rgroup != GC_REGISTER_GROUP_TEMP)
      return;

   ir->temp_start[var] = MIN2(ir->temp_start[var], ip);
   ir->temp_end[var] = MAX2(ir->temp_end[var], ip);

   /* The use[] bitset marks when the block makes
    * use of a variable without having completely
    * defined that variable within the block.
    */
   if (!BITSET_TEST(block->def, var))
      BITSET_SET(block->use, var);
}

static inline void
setup_def(struct eir_block *block, const struct gc_instr_dst *dst, int ip)
{
   const int var = dst->reg;
   const struct eir *ir = block->ir;

   ir->temp_start[var] = MIN2(ir->temp_start[var], ip);
   ir->temp_end[var] = MAX2(ir->temp_end[var], ip);

   /* If we've already tracked this as a def, or already used it within
    * the block, there's nothing to do.
    */
   if (BITSET_TEST(block->use, var) || BITSET_TEST(block->def, var))
      return;

   /* TODO: condinatinal code etc.. */
   BITSET_SET(block->def, var);
}

static void
setup_use_def_alu(struct eir_block *block, const struct gc_instr *instr, int ip)
{
   const struct gc_instr_alu *alu = &instr->alu;

   for (unsigned i = 0; i < 3; i++)
      setup_use(block, &alu->src[i], ip);

   if (gc_op_has_dst(instr->opcode))
      setup_def(block, &instr->dst, ip);
}

static void
setup_use_def_branch(struct eir_block *block, const struct gc_instr *instr, int ip)
{
   const struct gc_instr_branch *branch = &instr->branch;

   for (unsigned i = 0; i < 2; i++)
      setup_use(block, &branch->src[i], ip);
}

static void
setup_use_def_sampler(struct eir_block *block, const struct gc_instr *instr, int ip)
{
   const struct gc_instr_sampler *sampler = &instr->sampler;

   setup_use(block, &sampler->src, ip);
   setup_def(block, &instr->dst, ip);
}

static void
setup_def_use(struct eir *ir)
{
   int ip = 0;

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (const struct eir_instruction, instr, &block->instr_list, node) {
         const struct gc_instr *gc = &instr->gc;

         switch (gc->type) {
         case GC_OP_TYPE_ALU:
            setup_use_def_alu(block, gc, ip);
            break;

         case GC_OP_TYPE_BRANCH:
            setup_use_def_branch(block, gc, ip);
            break;

         case GC_OP_TYPE_SAMPLER:
            setup_use_def_sampler(block, gc, ip);
            break;

         default:
            unreachable("handle me");
            break;
         }

         ip++;
      }
   }
}

void
eir_calculate_live_intervals(struct eir *ir)
{
   const int num_temps = ir->num_temps;
   const int bitset_words = BITSET_WORDS(num_temps);

   ir->temp_start = rzalloc_array(ir, int, num_temps);
   ir->temp_end = rzalloc_array(ir, int, num_temps);

   for (int i = 0; i < num_temps; i++) {
      ir->temp_start[i] = (1 << 30);
      ir->temp_end[i] = -1;
   }

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      block->def = rzalloc_array(ir, BITSET_WORD, bitset_words);
      block->use = rzalloc_array(ir, BITSET_WORD, bitset_words);
      block->live_in = rzalloc_array(ir, BITSET_WORD, bitset_words);
      block->live_out = rzalloc_array(ir, BITSET_WORD, bitset_words);
   }

   setup_def_use(ir);

   /* TODO dataflow over multipbe blocks */

   ir->live_intervals_valid = true;
}
