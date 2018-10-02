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

#ifndef H_EIR
#define H_EIR

#include <assert.h>
#include "util/list.h"
#include "util/u_dynarray.h"
#include "util/u_math.h"
#include "vivante/gc/gc_instr.h"

#ifdef __cplusplus
extern "C"{
#endif

struct eir;
struct eir_block;

struct eir_register
{
   enum {
      EIR_REG_UNDEF = 0,
      EIR_REG_TEMP,
      EIR_REG_UNIFORM,
      EIR_REG_SAMPLER,
   } type;

   unsigned index;

   /* dst only: */
   unsigned writemask:4;
   /* src only: */
   unsigned swizzle:8;
   unsigned abs:1;
   unsigned neg:1;
};

struct eir_instruction
{
   struct eir_block *block;
   struct list_head node;  /* entry in eir_block's instruction list */

   /* Pre-register-allocation references to src/dst registers */
   struct eir_register dst;
   struct eir_register src[3];

   struct gc_instr gc;  /* instruction being wrapped. */
};

struct eir_block
{
   struct eir *ir;
   struct list_head node;  /* entry in eir's block list */
   struct list_head instr_list;

   unsigned num;

   struct eir_block *successors[2];

   /* per-pass extra block data */
   void *data;
};

enum eir_uniform_contents
{
   EIR_UNIFORM_CONSTANT,
};

struct eir_uniform_data
{
   enum eir_uniform_contents content;
   uint32_t data;
};

struct eir
{
   struct list_head block_list;

   unsigned blocks;

   /* keep track of numer of allocated registers
    * and the used components per register */
   struct util_dynarray reg_alloc;

   /* keep track of number of allocated uniforms */
   struct util_dynarray uniform_alloc;
   unsigned uniform_offset;
};

struct eir_info {
	uint16_t sizedwords;
};

struct eir *
eir_create(void);

void
eir_destroy(struct eir *ir);

struct eir_block *
eir_block_create(struct eir *ir);

struct eir_instruction *
eir_instruction_create(struct eir_block *block, enum gc_op opc);

struct eir_register
eir_temp_register(struct eir *ir, unsigned num_components);

struct eir_register
eir_uniform_register(struct eir *ir, enum eir_uniform_contents content, uint32_t value);

struct eir_register
eir_uniform_register_vec4(struct eir *ir, enum eir_uniform_contents content, const uint32_t *values);

static inline struct eir_register
eir_uniform_register_ui(struct eir *ir, uint32_t value)
{
   return eir_uniform_register(ir, EIR_UNIFORM_CONSTANT, value);
}

static inline struct eir_register
eir_uniform_register_f(struct eir *ir, float value)
{
   return eir_uniform_register(ir, EIR_UNIFORM_CONSTANT, fui(value));
}

static inline struct eir_register
eir_uniform_register_vec4_ui(struct eir *ir, const uint32_t *values)
{
   return eir_uniform_register_vec4(ir, EIR_UNIFORM_CONSTANT, values);
}

static inline struct eir_register
eir_uniform_register_vec4_f(struct eir *ir, float *values)
{
   uint32_t val[4];

   for (unsigned i = 0; i < 4; i++)
      val[i] = fui(values[i]);

   return eir_uniform_register_vec4(ir, EIR_UNIFORM_CONSTANT, val);
}

void
eir_link_blocks(struct eir_block *predecessor, struct eir_block *successor);

void
eir_legalize(struct eir *ir);

void
eir_calculate_live_intervals(struct eir *ir);

int
eir_temp_range_start(const struct eir* ir, unsigned n);

int
eir_temp_range_end(const struct eir* ir, unsigned n);

uint32_t *
eir_assemble(struct eir *ir, struct eir_info *info);

static inline struct eir_instruction *
eir_BRANCH(struct eir_block *block, struct eir_register *src0,
           struct eir_register *src1)
{
   struct eir_instruction *instr;

   assert(!gc_op_has_dst(GC_BRANCH));
   assert(gc_op_num_src(GC_BRANCH) == 2);

   instr = eir_instruction_create(block, GC_BRANCH);
   instr->gc.type = GC_OP_TYPE_BRANCH;
   instr->gc.branch.imm = ~0;
   instr->src[0] = *src0;
   instr->src[1] = *src1;

   return instr;
}

#define ALU0(opc) \
static inline struct eir_instruction * \
eir_##opc(struct eir_block *block) \
{ \
   struct eir_instruction *instr; \
   \
   assert(!gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 0);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->gc.type = GC_OP_TYPE_ALU; \
   \
   return instr; \
}

#define ALU1(opc) \
static inline struct eir_instruction * \
eir_##opc(struct eir_block *block, struct eir_register *dst, \
          struct eir_register *src0) \
{ \
   struct eir_instruction *instr; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) >= 1);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->gc.type = GC_OP_TYPE_ALU; \
   instr->dst = *dst; \
   instr->src[0] = *src0; \
   \
   return instr; \
}

#define ALU2(opc) \
static inline struct eir_instruction * \
eir_##opc(struct eir_block *block, struct eir_register *dst, \
          struct eir_register *src0, struct eir_register *src1) \
{ \
   struct eir_instruction *instr; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 2);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->gc.type = GC_OP_TYPE_ALU; \
   instr->dst = *dst; \
   instr->src[0] = *src0; \
   instr->src[1] = *src1; \
   \
   return instr; \
}

#define ALU3(opc) \
static inline struct eir_instruction * \
eir_##opc(struct eir_block *block, struct eir_register *dst, \
          struct eir_register *src0, struct eir_register *src1, \
          struct eir_register *src2) \
{ \
   struct eir_instruction *instr; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 3);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->gc.type = GC_OP_TYPE_ALU; \
   instr->dst = *dst; \
   instr->src[0] = *src0; \
   instr->src[1] = *src1; \
   instr->src[2] = *src2; \
   \
   return instr; \
}

#define TEX(opc) \
static inline struct eir_instruction * \
eir_##opc(struct eir_block *block, struct eir_register *dst, \
          struct eir_register *sampler, struct eir_register *src) \
{ \
   struct eir_instruction *instr; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 2); \
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->gc.type = GC_OP_TYPE_SAMPLER; \
   instr->dst = *dst; \
   instr->src[0] = *sampler; \
   instr->src[1] = *src; \
   \
   return instr; \
}

ALU0(NOP);
ALU0(TEXKILL);

ALU1(MOV);
ALU1(MOVAR);
ALU1(RCP);
ALU1(RSQ);
ALU1(CEIL);
ALU1(FLOOR);
ALU1(FRC);
ALU1(LOG);
ALU1(SQRT);
ALU1(SIN);
ALU1(COS);
ALU1(EXP);
ALU1(DSX);
ALU1(DSY);

ALU2(ADD);
ALU2(MUL);
ALU2(DP2);
ALU2(DP3);
ALU2(DP4);
ALU2(SET);
ALU0(TEXKILL_IF);

ALU3(MAD);
ALU3(SELECT);

TEX(TEXLD);
TEX(TEXLDB);
TEX(TEXLDL);

#define eir_for_each_block(block, ir)                                   \
   list_for_each_entry(struct eir_block, block, &ir->block_list, node)

#define eir_for_each_block_rev(block, ir)                               \
   list_for_each_entry_rev(struct eir_block, block, &ir->block_list, node)

/* Loop over the non-NULL members of the successors array. */
#define eir_for_each_successor(succ, block)                             \
   for (struct eir_block *succ = block->successors[0];                  \
         succ != NULL;                                                  \
         succ = (succ == block->successors[1] ? NULL :                  \
               block->successors[1]))

#define eir_for_each_inst(inst, block)                                  \
   list_for_each_entry(struct eir_instruction, inst, &block->instr_list, node)

#ifdef __cplusplus
}
#endif

#endif // H_EIR
