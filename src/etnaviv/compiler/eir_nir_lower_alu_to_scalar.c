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

#include "eir_nir.h"
#include "nir.h"
#include "nir_builder.h"

/** @file eir_nir_lower_to_scalar.c
 *
 * Replaces nir alu operations with individual per-channel operations.
 */

static void
nir_alu_ssa_dest_init(nir_alu_instr *instr, unsigned num_components,
                      unsigned bit_size)
{
   nir_ssa_dest_init(&instr->instr, &instr->dest.dest, num_components,
                     bit_size, NULL);
   instr->dest.write_mask = (1 << num_components) - 1;
}

static nir_ssa_def *
replace(nir_builder *b, nir_alu_instr *instr)
{
   unsigned num_src = nir_op_infos[instr->op].num_inputs;
   unsigned num_components = instr->dest.dest.ssa.num_components;
   nir_ssa_def *comps[NIR_MAX_VEC_COMPONENTS] = { NULL };
   unsigned i, chan;

   for (chan = 0; chan < NIR_MAX_VEC_COMPONENTS; chan++) {
      if (!(instr->dest.write_mask & (1 << chan)))
         continue;

      nir_alu_instr *lower = nir_alu_instr_create(b->shader, instr->op);
      for (i = 0; i < num_src; i++) {
         /* We only handle same-size-as-dest (input_sizes[] == 0) or scalar
          * args (input_sizes[] == 1).
          */
         assert(nir_op_infos[instr->op].input_sizes[i] < 2);
         unsigned src_chan = (nir_op_infos[instr->op].input_sizes[i] == 1 ?
                              0 : chan);

         nir_alu_src_copy(&lower->src[i], &instr->src[i], lower);
         for (int j = 0; j < NIR_MAX_VEC_COMPONENTS; j++)
            lower->src[i].swizzle[j] = instr->src[i].swizzle[src_chan];
      }

      nir_alu_ssa_dest_init(lower, 1, instr->dest.dest.ssa.bit_size);
      lower->dest.saturate = instr->dest.saturate;
      comps[chan] = &lower->dest.dest.ssa;
      lower->exact = instr->exact;

      nir_builder_instr_insert(b, &lower->instr);
   }

   return nir_vec(b, comps, num_components);
}

static void
lower_alu_to_scalar_impl(nir_function_impl *impl)
{
   bool progress = false;

   nir_builder b;
   nir_builder_init(&b, impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_alu)
            continue;

         nir_alu_instr *alu_instr = nir_instr_as_alu(instr);
         nir_ssa_def *vec;

         b.cursor = nir_before_instr(instr);

         switch (alu_instr->op) {
         case nir_op_fexp2:
            /* fall-through */
         case nir_op_flog2:
            vec = replace(&b, alu_instr);
            break;
         default:
            continue;
         }

         nir_ssa_def_rewrite_uses(&alu_instr->dest.dest.ssa,
                                  nir_src_for_ssa(vec));
         nir_instr_remove(instr);
         progress = true;
      }
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance);
   }
}

void
eir_nir_lower_alu_to_scalar(struct nir_shader *shader)
{
   nir_foreach_function(function, shader) {
      if (function->impl)
         lower_alu_to_scalar_impl(function->impl);
   }
}
