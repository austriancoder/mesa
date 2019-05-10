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

#ifndef H_ETNAVIV_EIR
#define H_ETNAVIV_EIR

#include "etnaviv_shader.h"

#include "etnaviv/compiler/eir_shader.h"
#include "pipe/p_state.h"

struct etna_context;

struct eir_shader *
eir_shader_create(struct eir_compiler *compiler,
                  const struct pipe_shader_state *cso,
                  gl_shader_stage type,
                  struct pipe_debug_callback *debug,
                  struct pipe_screen *screen);

void *
eir_shader_variant(void *s,
                   struct etna_shader_key key,
                   struct pipe_debug_callback *debug);

struct nir_shader *
eir_tgsi_to_nir(const struct tgsi_token *tokens, struct pipe_screen *screen);

void
eir_uniforms_write(const struct etna_context *ctx,
                   const void *v,
                   struct pipe_constant_buffer *cb,
                   uint32_t *uniforms, unsigned *size);

uint32_t
eir_uniforms_const_count(const void *v);

uint32_t
eir_uniform_dirty_flags(const void *v);

bool
eir_link_shaders(struct etna_context *ctx);

bool
eir_shader_update_vertex(struct etna_context *ctx);

#endif
