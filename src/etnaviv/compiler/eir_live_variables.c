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
#include "util/bitset.h"
#include "util/ralloc.h"
#include "util/u_math.h"

#define MAX_INSTRUCTION (1 << 30)

struct block_data {
   BITSET_WORD *def;
   BITSET_WORD *use;
   BITSET_WORD *livein;
   BITSET_WORD *liveout;
   int start_ip, end_ip;
};

/* Returns the variable index for the c-th component of register reg. */
static inline unsigned
var_from_reg(unsigned reg, unsigned c)
{
   assert(c < 4);
   return (reg * 4) + c;
}

static void
setup_use(struct eir_block *block, const struct eir_register *src, int ip)
{
   const struct eir *ir = block->ir;
   struct block_data *bd = block->data;

   if (src->type != EIR_REG_TEMP)
      return;

   /* The use[] bitset marks when the block makes
    * use of a variable without having completely
    * defined that variable within the block.
    */

   const unsigned swiz_comp[4] = {
      /* x */ src->swizzle & 0x3,
      /* y */ (src->swizzle & 0x0C) >> 2,
      /* z */ (src->swizzle & 0x30) >> 4,
      /* w */ (src->swizzle & 0xc0) >> 6,
   };

   for (unsigned c = 0; c < 4; c++) {
      const unsigned var = var_from_reg(src->index, swiz_comp[c]);

      ir->temp_start[var] = MIN2(ir->temp_start[var], ip);
      ir->temp_end[var] = ip;

      if (!BITSET_TEST(bd->def, var))
         BITSET_SET(bd->use, var);
   }
}

static inline void
setup_def(struct eir_block *block, const struct eir_register *dst, int ip)
{
   const struct eir *ir = block->ir;
   struct block_data *bd = block->data;

   for (unsigned c = 0; c < 4; c++) {
      if (dst->writemask & (1 << c)) {
         const unsigned var = var_from_reg(dst->index, c);

         ir->temp_start[var] = MIN2(ir->temp_start[var], ip);
         ir->temp_end[var] = ip;

         BITSET_SET(bd->def, var);
      }
   }
}

static void
setup_def_use(struct eir *ir)
{
   int ip = 0;

   eir_for_each_block(block, ir) {
      struct block_data *bd = block->data;
      bd->start_ip = ip;

      eir_for_each_inst(instr, block) {
         const struct gc_instr *gc = &instr->gc;

         for (unsigned i = 0; i < gc_op_num_src(gc->opcode); i++)
            setup_use(block, &instr->src[i], ip);

         if (gc_op_has_dst(gc->opcode))
            setup_def(block, &instr->dst, ip);

         ip++;
      }
      bd->end_ip = ip;
   }
}

static bool
compute_live_variables(struct eir *ir, unsigned bitset_words)
{
   bool progress = false;

   eir_for_each_block_rev(block, ir) {
      struct block_data *bd = block->data;

      /* update liveout */
      eir_for_each_successor(succ, block) {
         struct block_data *sd = succ->data;

         for (unsigned i = 0; i < bitset_words; i++) {
            BITSET_WORD new_liveout =
               (sd->livein[i] & ~bd->liveout[i]);

            if (new_liveout) {
                  bd->liveout[i] |= new_liveout;
                  progress = true;
            }
         }
      }

      /* update livein */
      for (unsigned i = 0; i < bitset_words; i++) {
         BITSET_WORD new_livein =
                  (bd->use[i] | (bd->liveout[i] & ~bd->def[i]));

         if (new_livein & ~bd->livein[i]) {
            bd->livein[i] |= new_livein;
            progress = true;
         }
      }
   }

   return progress;
}

static void
compute_start_end(struct eir *ir, const int num_vars)
{
   /* extend intervals using analysis of control flow */
   eir_for_each_block(block, ir) {
      struct block_data *bd = block->data;

      for (int i = 0; i < num_vars; i++) {
         if (BITSET_TEST(bd->livein, i)) {
            ir->temp_start[i] = MIN2(ir->temp_start[i], bd->start_ip);
            ir->temp_end[i] = MAX2(ir->temp_end[i], bd->start_ip);
         }

         if (BITSET_TEST(bd->liveout, i)) {
            ir->temp_start[i] = MIN2(ir->temp_start[i], bd->end_ip);
            ir->temp_end[i] = MAX2(ir->temp_end[i], bd->end_ip);
         }
      }
   }
}

void
eir_calculate_live_intervals(struct eir *ir)
{
   const int num = util_dynarray_num_elements(&ir->reg_alloc, unsigned);
   if (num == 0)
      return;

   assert(!ir->temp_start);
   assert(!ir->temp_end);

   const int num_components = num * 4;
   const unsigned bitset_words = BITSET_WORDS(num_components);

   ir->temp_start = rzalloc_array(ir, int, num_components);
   ir->temp_end = rzalloc_array(ir, int, num_components);

   for (int i = 0; i < num_components; i++) {
      ir->temp_start[i] = MAX_INSTRUCTION;
      ir->temp_end[i] = -1;
   }

   void *mem_ctx = ralloc_context(NULL);
   assert(mem_ctx);

   eir_for_each_block(block, ir) {
      struct block_data *bd = rzalloc(mem_ctx, struct block_data);

      assert(bd);
      assert(!block->data);

      bd->def = rzalloc_array(bd, BITSET_WORD, bitset_words);
      bd->use = rzalloc_array(bd, BITSET_WORD, bitset_words);
      bd->livein = rzalloc_array(bd, BITSET_WORD, bitset_words);
      bd->liveout = rzalloc_array(bd, BITSET_WORD, bitset_words);

      block->data = bd;
   }

   setup_def_use(ir);
   while (compute_live_variables(ir, bitset_words)) {}

   compute_start_end(ir, num_components);

   ralloc_free(mem_ctx);
   eir_for_each_block(block, ir)
      block->data = NULL;
}

int
eir_temp_range_start(const struct eir* ir, unsigned n)
{
   int start = INT_MAX;

   if (!ir->temp_start)
      return start;

   for (unsigned i = 0; i < 4; i++)
      start = MIN2(start, ir->temp_start[(n * 4) + i]);

   return start;
}

int
eir_temp_range_end(const struct eir *ir, unsigned n)
{
   int end = INT_MIN;

   if (!ir->temp_end)
      return end;

   for (unsigned i = 0; i < 4; i++)
      end = MAX2(end, ir->temp_end[(n * 4) + i]);

   return end;
}
