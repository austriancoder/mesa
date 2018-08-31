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

#include "eir_nir.h"
#include "compiler/nir/nir.h"

static const nir_shader_compiler_options options = {
   .lower_all_io_to_temps = true,
   .fdot_replicates = true,
   .fuse_ffma = true,
   .lower_sub = true,
   .lower_fpow = true,
   .lower_flrp32 = true,
   .max_unroll_iterations = 32
};

const struct nir_shader_compiler_options *
eir_get_compiler_options(void)
{
   return &options;
}

#define OPT(nir, pass, ...) ({                             \
   bool this_progress = false;                             \
   NIR_PASS(this_progress, nir, pass, ##__VA_ARGS__);      \
   this_progress;                                          \
})

#define OPT_V(nir, pass, ...) NIR_PASS_V(nir, pass, ##__VA_ARGS__)

static void
eir_optimize_loop(struct nir_shader *s)
{
   bool progress;
   do {
      progress = false;

      OPT_V(s, nir_lower_vars_to_ssa);
      progress |= OPT(s, nir_opt_copy_prop_vars);
      progress |= OPT(s, nir_copy_prop);
      progress |= OPT(s, nir_opt_dce);
      progress |= OPT(s, nir_opt_cse);
      progress |= OPT(s, nir_opt_peephole_select, 16);
      progress |= OPT(s, nir_opt_intrinsics);
      progress |= OPT(s, nir_opt_algebraic);
      progress |= OPT(s, nir_opt_constant_folding);
      progress |= OPT(s, nir_opt_dead_cf);
      if (OPT(s, nir_opt_trivial_continues)) {
         progress |= true;
         /* If nir_opt_trivial_continues makes progress, then we need to clean
          * things up if we want any hope of nir_opt_if or nir_opt_loop_unroll
          * to make progress.
          */
         OPT(s, nir_copy_prop);
         OPT(s, nir_opt_dce);
      }
      progress |= OPT(s, nir_opt_if);

      if (s->options->max_unroll_iterations)
         progress |= OPT(s, nir_opt_loop_unroll, 0);

      progress |= OPT(s, nir_opt_remove_phis);
      progress |= OPT(s, nir_opt_undef);

   } while (progress);
}

struct nir_shader *
eir_optimize_nir(struct nir_shader *s)
{
   struct nir_lower_tex_options tex_options = {
      .lower_txp = ~0,
      .lower_rect = true,
   };

   OPT_V(s, nir_lower_tex, &tex_options);
   OPT_V(s, nir_lower_global_vars_to_local);
   OPT_V(s, nir_lower_regs_to_ssa);

   OPT_V(s, nir_opt_algebraic);

   eir_optimize_loop(s);

   OPT_V(s, nir_remove_dead_variables, nir_var_local);

   OPT_V(s, nir_opt_algebraic_late);

   OPT_V(s, nir_lower_to_source_mods, nir_lower_all_source_mods);
   OPT_V(s, nir_copy_prop);
   OPT_V(s, nir_opt_dce);
   OPT_V(s, nir_opt_move_comparisons);

   OPT_V(s, nir_lower_locals_to_regs);
   OPT_V(s, nir_convert_from_ssa, true);
   OPT_V(s, nir_move_vec_src_uses_to_dest);
   OPT_V(s, nir_lower_vec_to_movs);

   nir_sweep(s);

   return s;
}
