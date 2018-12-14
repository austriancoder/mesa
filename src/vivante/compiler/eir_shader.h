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

#ifndef H_EIR_SHADER
#define H_EIR_SHADER

#include "compiler/glsl_types.h"
#include "compiler/shader_enums.h"
#include "eir.h"
#include <stdbool.h>
#include "util/macros.h"

struct nir_shader;

struct eir_shader_key
{
   union {
      struct {
         /*
          * Combined Vertex/Fragment shader parameters:
          */

         /* do we need to swap rb in frag color? */
         unsigned frag_rb_swap : 1;
      };
      uint32_t global;
   };
};

struct eir_shader;

struct eir_shader_variant
{
   /* variant id (for debug) */
   uint32_t id;

   struct eir_shader *shader;
   struct eir_shader_key key;
   struct eir_info info;   // TODO: really needed? would like to kill eir.h include
   struct eir *ir;

   uint32_t *code;

   unsigned num_temps;
   unsigned const_size;

   /* keep track of uniforms */
   struct util_dynarray uniforms;

   /* special outputs (vs only) */
   int vs_pos_out_reg; /* VS position output */
   int vs_pointsize_out_reg; /* VS point size output */

   /* special outputs (fs only) */
   int fs_color_out_reg; /* color output register */
   int fs_depth_out_reg; /* depth output register */

   /* shader variants form a linked list */
   struct eir_shader_variant *next;
};

struct eir_shader
{
   void *mem_ctx;
   gl_shader_stage type;

   /* shader id (for debug): */
   uint32_t id;
   uint32_t variant_count;

   struct eir_compiler *compiler;
   struct nir_shader *nir;
   struct eir_shader_variant *variants;
};

struct pipe_debug_callback;
struct pipe_shader_state;

struct eir_shader *
eir_shader_create(struct eir_compiler *compiler,
                  const struct pipe_shader_state *cso,
                  gl_shader_stage type,
                  struct pipe_debug_callback *debug);

void
eir_shader_destroy(struct eir_shader *shader);

struct eir_shader_variant *
eir_shader_variant(struct eir_shader *shader, struct eir_shader_key key,
                   struct pipe_debug_callback *debug);

void
eir_dump_shader(struct eir_shader_variant *variant);

int
eir_type_size_vec4(const struct glsl_type *type);

#endif // H_EIR_SHADER
