/*
 * Copyright (c) 2017 Etnaviv Project
 * Copyright (C) 2017 Zodiac Inflight Innovations
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

#ifndef H_EIR_NIR
#define H_EIR_NIR

#include <stdbool.h>

struct eir_compiler;
struct eir_shader;
struct nir_shader;
struct nir_shader_compiler_options;
struct tgsi_token;

const struct nir_shader_compiler_options *
eir_get_compiler_options(void);

struct nir_shader *
eir_tgsi_to_nir(const struct tgsi_token *tokens);

struct nir_shader *
eir_optimize_nir(struct eir_shader *shader, struct nir_shader *s);

bool
eir_nir_lower_uniforms(struct nir_shader *shader);

bool
eir_nir_legalize_uniforms(struct nir_shader *shader);

void
nir_fixup_register(struct nir_shader *shader);

#endif // H_EIR_NIR
