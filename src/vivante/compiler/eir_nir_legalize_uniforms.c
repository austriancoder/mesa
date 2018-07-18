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

/*
 * TODO: XXX
 */

#include "compiler/nir/nir.h"
#include "eir_nir.h"

static void
insert_mov(nir_instr *instr, nir_src *src, nir_shader *shader)
{
   unsigned num_components, max_swizzle;

   nir_alu_instr *mov = nir_alu_instr_create(shader, nir_op_fmov);
   nir_src_copy(&mov->src[0].src, src, &mov->instr);

   num_components = nir_src_num_components(mov->src[0].src);
   max_swizzle = num_components - 1;
   mov->src[0].swizzle[0] = 0;
   mov->src[0].swizzle[1] = MIN2(max_swizzle, 1);
   mov->src[0].swizzle[2] = MIN2(max_swizzle, 2);
   mov->src[0].swizzle[3] = MIN2(max_swizzle, 3);
   mov->dest.write_mask = (1 << num_components) - 1;
   nir_ssa_dest_init(&mov->instr, &mov->dest.dest, 4, 32, NULL);
   nir_instr_rewrite_src(instr, src,
                         nir_src_for_ssa(&mov->dest.dest.ssa));
   nir_instr_insert_before(instr, &mov->instr);
}

static bool
legalize_multiple_alu_uniforms(nir_instr *instr, struct nir_shader *shader)
{
   if (instr->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *alu = nir_instr_as_alu(instr);
   const unsigned num_inputs = nir_op_infos[alu->op].num_inputs;

   for (unsigned i = 0; i < num_inputs; i++) {
      nir_src *src = &alu->src[i].src;

      if (!src->is_ssa || !nir_src_is_dynamically_uniform(*src))
         continue;
      
      bool is_duplicate = false;
      for (unsigned j = 0; j < i; j++) {
         nir_src *other = &alu->src[j].src;

         if (other->is_ssa &&
             nir_src_is_dynamically_uniform(*other) &&
             other->ssa->parent_instr == src->ssa->parent_instr) {
            is_duplicate = true;
            break;
         }

         if (is_duplicate)
            continue;
         
         insert_mov(&alu->instr, src, shader);
         return true;
      }
   }

   return false;
}

static bool
legalize_uniform_outputs(nir_instr *instr, struct nir_shader *shader)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   if (intr->intrinsic != nir_intrinsic_store_output)
      return false;
   
   if (!nir_src_is_dynamically_uniform(intr->src[0]))
      return false;

   insert_mov(&intr->instr, &intr->src[0], shader);

   return true;
}

bool
eir_nir_legalize_uniforms(struct nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            progress |= legalize_multiple_alu_uniforms(instr, shader);
            progress |= legalize_uniform_outputs(instr, shader);
         }
      }

      nir_metadata_preserve(function->impl, nir_metadata_block_index);
   }

   return progress;
}
