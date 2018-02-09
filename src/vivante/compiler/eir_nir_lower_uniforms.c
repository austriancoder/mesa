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
 * Remap load_uniform intrinsics to global registers.
 */

#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "eir_nir.h"

static bool
lower_instr(nir_intrinsic_instr *instr, nir_builder *b)
{
   b->cursor = nir_before_instr(&instr->instr);

   if (instr->intrinsic == nir_intrinsic_load_uniform) {
      nir_register *reg = nir_global_reg_create(b->shader);
      reg->num_components = instr->dest.ssa.num_components;

      nir_ssa_def_rewrite_uses(&instr->dest.ssa, nir_src_for_reg(reg));
      nir_instr_remove(&instr->instr);

      return true;
   }

   return false;
}

bool
eir_nir_lower_uniforms(struct nir_shader *shader)
{
   bool progress = false;

   assert(shader->reg_alloc == 0);

   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      nir_builder builder;
      nir_builder_init(&builder, function->impl);

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type == nir_instr_type_intrinsic)
               progress |= lower_instr(nir_instr_as_intrinsic(instr),
                                       &builder);
         }
      }

      nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                             nir_metadata_dominance);
   }

   return progress;
}
