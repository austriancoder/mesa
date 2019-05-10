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
#include <util/u_debug.h>

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
   struct eir_info info;
   struct eir *ir;

   uint32_t *code;

   unsigned num_temps;
   unsigned num_loops;
   unsigned const_size;

   /* keep track of uniforms */
   struct util_dynarray uniforms;

   /* attributes/varyings: */
   unsigned num_inputs;
   struct {
      uint8_t reg;
      uint8_t slot;
      uint8_t ncomp;

      enum glsl_interp_mode interpolate;
   } inputs[16];

   /* varyings/outputs: */
   unsigned num_outputs;
   struct {
      uint8_t reg;
      uint8_t slot;
      uint8_t ncomp;
   } outputs[16];

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

struct eir_shader *
eir_shader_from_nir(struct eir_compiler *compiler, struct nir_shader *nir);

void
eir_shader_destroy(struct eir_shader *shader);

struct eir_shader_variant *
eir_shader_get_variant(struct eir_shader *shader, struct eir_shader_key key, bool *created);

void
eir_dump_shader(struct eir_shader_variant *variant);

struct eir_shader_linkage {
   uint8_t num_varyings;
   struct {
      uint32_t pa_attributes;
      uint8_t ncomp;
      uint8_t use[4];
      uint8_t reg;
   } varyings[16];
};

static inline int
eir_find_output(const struct eir_shader_variant *v, gl_varying_slot slot)
{
	for (unsigned i = 0; i < v->num_outputs; i++)
		if (v->outputs[i].slot == slot)
			return i;

   debug_assert(0);

   return 0;
}

/* TODO: */
#define VARYING_COMPONENT_USE_UNUSED				0x00000000
#define VARYING_COMPONENT_USE_USED				0x00000001
#define VARYING_COMPONENT_USE_POINTCOORD_X			0x00000002
#define VARYING_COMPONENT_USE_POINTCOORD_Y			0x00000003

static inline void
eir_link_add(struct eir_shader_linkage *l, uint8_t reg, uint8_t ncomp)
{
   bool interpolate_always = false; /* TODO: */
   uint32_t pa_attributes;
   int i = l->num_varyings++;

   debug_assert(i < ARRAY_SIZE(l->varyings));

   if (!interpolate_always) /* colors affected by flat shading */
      pa_attributes = 0x200;
   else /* texture coord or other bypasses flat shading */
      pa_attributes = 0x2f1;

   l->varyings[i].reg = reg;
   l->varyings[i].ncomp = ncomp;
   l->varyings[i].pa_attributes = pa_attributes;

   l->varyings[i].use[0] = interpolate_always ? VARYING_COMPONENT_USE_POINTCOORD_X : VARYING_COMPONENT_USE_USED;
   l->varyings[i].use[1] = interpolate_always ? VARYING_COMPONENT_USE_POINTCOORD_Y : VARYING_COMPONENT_USE_USED;
   l->varyings[i].use[2] = VARYING_COMPONENT_USE_USED;
   l->varyings[i].use[3] = VARYING_COMPONENT_USE_USED;
}

static inline void
eir_link_shader(struct eir_shader_linkage *l,
   const struct eir_shader_variant *vs,
   const struct eir_shader_variant *fs)
{
   int i = 0;

   while (l->num_varyings < ARRAY_SIZE(l->varyings)) {

		if (i >= fs->num_inputs)
			break;

      int reg = eir_find_output(vs, fs->inputs[i].slot);

      eir_link_add(l, reg, fs->inputs[i].ncomp);

      i++;
   }
}

#endif // H_EIR_SHADER
