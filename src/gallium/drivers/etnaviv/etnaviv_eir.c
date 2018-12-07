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

#include "nir/tgsi_to_nir.h"

#include "util/u_memory.h"
#include "vivante/compiler/eir_nir.h"
#include "vivante/compiler/eir_shader.h"
#include "pipe/p_state.h"

static void
dump_shader_info(struct eir_shader_variant *v, struct pipe_debug_callback *debug)
{
   //if (!unlikely(v->shader->compiler->debug & EIR_DBG_SHADERDB))
      return;
/*
   pipe_debug_message(debug, SHADER_INFO, "\n"
         "SHADER-DB: %s prog %d/%d: %u instructions %u temps\n"
         "SHADER-DB: %s prog %d/%d: %u immediates %u consts\n"
         "SHADER-DB: %s prog %d/%d: %u loops\n",
         eir_shader_stage(v->shader),
         v->shader->id, v->id,
         v->code_size,
         v->num_temps,
         eir_shader_stage(v->shader),
         v->shader->id, v->id,
         v->uniforms.imm_count,
         v->uniforms.const_count,
         eir_shader_stage(v->shader),
         v->shader->id, v->id,
         v->num_loops);
*/
}

struct eir_shader *
eir_shader_create(struct eir_compiler *compiler,
                  const struct pipe_shader_state *cso,
                  gl_shader_stage type,
                  struct pipe_debug_callback *debug)
{
   struct nir_shader *nir;

   assert(compiler);

   if (cso->type == PIPE_SHADER_IR_NIR) {
      /* we take ownership of the reference */
      nir = cso->ir.nir;
   } else {
      debug_assert(cso->type == PIPE_SHADER_IR_TGSI);
/*
      if (compiler->debug & EIR_DBG_OPTMSGS) {
         //DBG("dump tgsi: type=%d", shader->type);
         tgsi_dump(cso->tokens, 0);
      }
*/
      nir = eir_tgsi_to_nir(cso->tokens);
   }

   struct eir_shader *shader = eir_shader_from_nir(compiler, nir);

   /*if (compiler->debug & EIR_DBG_SHADERDB)*/ {
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
eir_tgsi_to_nir(const struct tgsi_token *tokens)
{
   return tgsi_to_nir(tokens, eir_get_compiler_options());
}
