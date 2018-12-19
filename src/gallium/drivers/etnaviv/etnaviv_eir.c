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

#include "etnaviv_eir.h"

#include "etnaviv_context.h"
#include "etnaviv_debug.h"
#include "nir/tgsi_to_nir.h"
#include "tgsi/tgsi_dump.h"
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "etnaviv/compiler/eir_compiler.h"
#include "etnaviv/compiler/eir_nir.h"
#include "etnaviv/compiler/eir_shader.h"
#include "pipe/p_state.h"

static void
dump_shader_info(struct eir_shader_variant *v, struct pipe_debug_callback *debug)
{
   if (!unlikely(etna_mesa_debug & ETNA_DBG_SHADERDB))
      return;

   pipe_debug_message(debug, SHADER_INFO, "\n"
         "SHADER-DB: %s prog %d/%d: %u instructions %u temps\n"
         "SHADER-DB: %s prog %d/%d: %lu immediates %u consts\n"
         "SHADER-DB: %s prog %d/%d: %u loops\n",
         gl_shader_stage_name(v->shader->type),
         v->shader->id, v->id,
         v->info.sizedwords,
         v->num_temps,
         gl_shader_stage_name(v->shader->type),
         v->shader->id, v->id,
         util_dynarray_num_elements(&v->uniforms, struct eir_uniform_data),
         v->const_size,
         gl_shader_stage_name(v->shader->type),
         v->shader->id, v->id,
         v->num_loops);
}

struct eir_shader *
eir_shader_create(struct eir_compiler *compiler,
                  const struct pipe_shader_state *cso,
                  gl_shader_stage type,
                  struct pipe_debug_callback *debug,
                  struct pipe_screen *screen)
{
   struct nir_shader *nir;

   assert(compiler);

   if (cso->type == PIPE_SHADER_IR_NIR) {
      /* we take ownership of the reference */
      nir = cso->ir.nir;
   } else {
      debug_assert(cso->type == PIPE_SHADER_IR_TGSI);

      if (eir_compiler_debug & EIR_DBG_DISASM)
         tgsi_dump(cso->tokens, 0);

      nir = eir_tgsi_to_nir(cso->tokens, screen);
   }

   struct eir_shader *shader = eir_shader_from_nir(compiler, nir);

   if (etna_mesa_debug & ETNA_DBG_SHADERDB) {
      /* if shader-db run, create a standard variant immediately
       * (as otherwise nothing will trigger the shader to be
       * actually compiled)
       */
      static struct eir_shader_key key;
      memset(&key, 0, sizeof(key));
      eir_shader_variant(shader, key, debug);
   }

   return shader;
}

struct eir_shader_variant *
eir_shader_variant(struct eir_shader *shader, struct eir_shader_key key,
                   struct pipe_debug_callback *debug)
{
   struct eir_shader_variant *v;
   bool created = false;

   v = eir_shader_get_variant(shader, key, &created);

   if (created)
      dump_shader_info(v, debug);

   return v;
}

struct nir_shader *
eir_tgsi_to_nir(const struct tgsi_token *tokens, struct pipe_screen *screen)
{
   if (!screen) {
      const nir_shader_compiler_options *options = eir_get_compiler_options();

      return tgsi_to_nir_noscreen(tokens, options);
   }

   return tgsi_to_nir(tokens, screen);
}

static inline unsigned
get_const_idx(const struct etna_context *ctx, bool frag, unsigned samp_id)
{
   if (frag)
      return samp_id;

   return samp_id + ctx->specs.vertex_sampler_offset;
}

static uint32_t
get_texture_size(const struct etna_context *ctx,
                 const struct eir_shader_variant *variant,
                 struct eir_uniform_data *uniform)
{
   const bool frag = ctx->shader.fs == variant;
   const unsigned index = get_const_idx(ctx, frag, uniform->data);
   const struct pipe_sampler_view *texture = ctx->sampler_view[index];

   switch (uniform->content) {
   case EIR_UNIFORM_IMAGE_WIDTH:
      return texture->texture->width0;
   case EIR_UNIFORM_IMAGE_HEIGHT:
      return texture->texture->height0;
   case EIR_UNIFORM_IMAGE_DEPTH:
      return texture->texture->depth0;
   default:
      unreachable("Bad texture size field");
   }
}

void
eir_uniforms_write(const struct etna_context *ctx,
                   const struct pipe_constant_buffer *cb,
                   const struct eir_shader_variant *variant,
                   uint32_t *uniforms, unsigned *size)
{
   const unsigned offset = MIN2(cb->buffer_size, variant->const_size);
   unsigned i = offset;

   if (cb->user_buffer)
      memcpy(uniforms, cb->user_buffer, offset * 4);

   util_dynarray_foreach(&variant->uniforms, struct eir_uniform_data, uniform) {
      switch (uniform->content) {
      case EIR_UNIFORM_UNUSED:
         uniforms[i] = 0;
         break;
      case EIR_UNIFORM_CONSTANT:
         uniforms[i] = uniform->data;
         break;
      case EIR_UNIFORM_IMAGE_WIDTH:
         /* fall-through */
      case EIR_UNIFORM_IMAGE_HEIGHT:
         /* fall-through */
      case EIR_UNIFORM_IMAGE_DEPTH:
         uniforms[i] = get_texture_size(ctx, variant, uniform);
         break;
      default:
         unreachable("unhandled content type");
         break;
      }

      i++;
   }

   *size = i;
}
