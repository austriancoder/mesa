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
#include "eir_compiler.h"
#include "util/ralloc.h"
#include "util/register_allocate.h"

static const int MAX_TEMP = 64;

/* register-set, created one time, used for all shaders */
struct eir_ra_reg_set {
   struct ra_regs *regs;
   int class;
};

struct eir_ra_reg_set *
eir_ra_alloc_reg_set(void *memctx)
{
   struct eir_ra_reg_set *set = rzalloc(memctx, struct eir_ra_reg_set);

   set->regs = ra_alloc_reg_set(set, MAX_TEMP, true);
   set->class = ra_alloc_reg_class(set->regs);

   for (unsigned i = 0; i < MAX_TEMP; i++)
      ra_class_add_reg(set->regs, set->class, i);

   ra_set_finalize(set->regs, NULL);

   return set;
}

/* does it conflict? */
static inline bool
intersects(unsigned a_start, unsigned a_end, unsigned b_start, unsigned b_end)
{
   return !((a_start >= b_end) || (b_start >= a_end));
}

static void
assign_register_src(struct gc_instr_src *src, struct ra_graph *g)
{
   if (!src->use)
      return;

   if (src->rgroup != GC_REGISTER_GROUP_TEMP)
      return;

   src->reg = ra_get_node_reg(g, src->reg);
}

static void
assign_register_dst(struct gc_instr_dst *dst, struct ra_graph *g)
{
   dst->reg = ra_get_node_reg(g, dst->reg);
}

static void
assign_register_alu(struct gc_instr *instr, struct ra_graph *g)
{
   struct gc_instr_alu *alu = &instr->alu;

   for (unsigned i = 0; i < 3; i++)
      assign_register_src(&alu->src[i], g);

   if (gc_op_has_dst(instr->opcode))
      assign_register_dst(&instr->dst, g);
}

static void
assign_resgier_branch(struct gc_instr *instr, struct ra_graph *g)
{
   struct gc_instr_branch *branch = &instr->branch;

   for (unsigned i = 0; i < 2; i++)
      assign_register_src(&branch->src[i], g);
}

static void
assign_register_sampler(struct gc_instr *instr, struct ra_graph *g)
{
   struct gc_instr_sampler *sampler = &instr->sampler;

   assign_register_src(&sampler->src, g);
   assign_register_dst(&instr->dst, g);
}

bool
eir_register_allocate(struct eir *ir, struct eir_compiler *compiler)
{
   const int num = ir->num_temps;
   struct ra_graph *g = ra_alloc_interference_graph(compiler->set->regs, num);
   int used_temps;
   bool success;

   /* TODO: pre-color some reserved regs with ra_set_node_reg(..) */

   for (unsigned i = 0; i < num; i++) {
      for (unsigned j = i + 1; j < num; j++) {
         if (intersects(ir->temp_start[i], ir->temp_end[j],
             ir->temp_start[j], ir->temp_end[i])) {
               ra_add_node_interference(g, i, j);
         }
      }
   }

   for (unsigned i = 0; i < num; i++)
      ra_set_node_class(g, i, compiler->set->class);

   success = ra_allocate(g);
   if (!success)
      goto out;

   /* assign register numbers */
   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (struct eir_instruction, instr, &block->instr_list, node) {
         struct gc_instr *gc = &instr->gc;

         switch (gc->type) {
         case GC_OP_TYPE_ALU:
            assign_register_alu(gc, g);
            break;

         case GC_OP_TYPE_BRANCH:
            assign_resgier_branch(gc, g);
            break;

         case GC_OP_TYPE_SAMPLER:
            assign_register_sampler(gc, g);
            break;

         default:
            unreachable("handle me");
            break;
         }
      }
   }

   ir->live_intervals_valid = false;

   /* update num_temp */
   used_temps = 0;
   for (unsigned i = 0; i < num; i++)
      used_temps = MAX2(used_temps, ra_get_node_reg(g, i) + 1);

   ir->num_temps = used_temps;

out:
   ralloc_free(g);

   return success;
}
