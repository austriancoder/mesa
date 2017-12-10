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

#ifndef H_EIR
#define H_EIR

#include "etnaviv_asm.h"
#include "etnaviv_context.h"
#include "util/u_math.h"

/* XXX some of these are pretty arbitrary limits, may be better to switch
 * to dynamic allocation at some point.
 */
#define ETNA_MAX_TEMPS (64) /* max temp register count of all Vivante hw */
#define ETNA_MAX_TOKENS (2048)
#define ETNA_MAX_IMM (1024) /* max const+imm in 32-bit words */
#define ETNA_MAX_DECL (2048) /* max declarations */
#define ETNA_MAX_DEPTH (32)
#define ETNA_MAX_INSTRUCTIONS (2048)

struct eir
{
   /* Immediate data */
   enum etna_immediate_contents imm_contents[ETNA_MAX_IMM];
   uint32_t imm_data[ETNA_MAX_IMM];
   uint32_t imm_base; /* base of immediates (in 32 bit units) */
   uint32_t imm_size; /* size of immediates (in 32 bit units) */
};

struct eir *
eir_create(void);

void
eir_destroy(struct eir *ir);


struct etna_inst_src
eir_immediate(struct eir *ir, enum etna_immediate_contents contents,
              uint32_t data);

struct etna_inst_src
eir_immediate_vec4(struct eir *ir, enum etna_immediate_contents contents,
                   const uint32_t *values);

static inline struct etna_inst_src
eir_immediate_u32(struct eir *ir, uint32_t ui)
{
   return eir_immediate(ir, ETNA_IMMEDIATE_CONSTANT, ui);
}

static inline struct etna_inst_src
eir_immediate_f32(struct eir *ir, float f)
{
   return eir_immediate(ir, ETNA_IMMEDIATE_CONSTANT, fui(f));
}

#endif // H_EIR
