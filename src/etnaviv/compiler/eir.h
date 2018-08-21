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
#include "etnaviv/gc/gc_instr.h"

#ifdef __cplusplus
extern "C"{
#endif

struct eir;
struct eir_block;
struct eir_ra_reg_set;

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
   EIR_UNIFORM_UNUSED,
   EIR_UNIFORM_CONSTANT,
   EIR_UNIFORM_IMAGE_WIDTH,
   EIR_UNIFORM_IMAGE_HEIGHT,
   EIR_UNIFORM_IMAGE_DEPTH,
};

static inline const char *
eir_uniform_content(enum eir_uniform_contents content)
{
   switch(content) {
   case EIR_UNIFORM_UNUSED:
      return "unused";
   case EIR_UNIFORM_CONSTANT:
      return "constant";
   case EIR_UNIFORM_IMAGE_WIDTH:
      return "image width";
   case EIR_UNIFORM_IMAGE_HEIGHT:
      return "image height";
   case EIR_UNIFORM_IMAGE_DEPTH:
      return "image depth";
   default:
      unreachable("unhandled content type");
      break;
   }

   return NULL;
}

struct eir_uniform_data
{
   enum eir_uniform_contents content;
   uint32_t data;
};

struct eir
{
   struct list_head block_list;
   unsigned blocks;

   /* attributes/varyings: */
   unsigned num_inputs;
   struct {
      unsigned slot;
      unsigned reg;
      unsigned ncomp;
   } inputs[16];

   /* varyings/outputs: */
   unsigned num_outputs;
   struct {
      unsigned slot;
      unsigned reg;
      unsigned ncomp;
   } outputs[16];

   /* keep track of numer of allocated registers
    * and the used components per register */
   struct util_dynarray reg_alloc;

   /* keep track of number of allocated uniforms */
   struct util_dynarray uniform_alloc;
   unsigned uniform_offset;

   /* Live ranges of temp registers */
   int *temp_start, *temp_end;
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
eir_uniform_register_vec4(struct eir *ir,
                          const unsigned ncomp,
                          const enum eir_uniform_contents *contents,
                          const uint32_t *values);

static inline struct eir_register
eir_uniform_register(struct eir *ir, const enum eir_uniform_contents content, uint32_t value)
{
   const enum eir_uniform_contents contents[] = {
      content
   };

   const uint32_t values[] = {
      value
   };

   return eir_uniform_register_vec4(ir, 1, contents, values);
}

static inline struct eir_register
eir_uniform_register_ui(struct eir *ir, uint32_t value)
{
   static const enum eir_uniform_contents contents[] = {
      EIR_UNIFORM_CONSTANT
   };

   uint32_t values[] = {
      value
   };

   return eir_uniform_register_vec4(ir, 1, contents, values);
}

static inline struct eir_register
eir_uniform_register_f(struct eir *ir, float value)
{
   static const enum eir_uniform_contents contents[] = {
      EIR_UNIFORM_CONSTANT
   };

   uint32_t values[] = {
      fui(value)
   };

   return eir_uniform_register_vec4(ir, 1, contents, values);
}

static inline struct eir_register
eir_uniform_register_vec4_ui(struct eir *ir, const unsigned ncomp, const uint32_t *values)
{
   static const enum eir_uniform_contents contents[] =
   {
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
   };

   return eir_uniform_register_vec4(ir, ncomp, contents, values);
}

static inline struct eir_register
eir_uniform_register_vec4_f(struct eir *ir, const unsigned ncomp, float *values)
{
   static const enum eir_uniform_contents contents[] =
   {
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
   };

   uint32_t val[4];

   for (unsigned i = 0; i < ncomp; i++)
      val[i] = fui(values[i]);

   return eir_uniform_register_vec4(ir, ncomp, contents, val);
}

void
eir_link_blocks(struct eir_block *predecessor, struct eir_block *successor);

static inline void
eir_assign_input(struct eir *ir, unsigned idx, unsigned slot, unsigned ncomp)
{
   assert(ir);
   assert(idx < ARRAY_SIZE(ir->inputs));

   ir->inputs[idx].reg = idx;
   ir->inputs[idx].slot = slot;
   ir->inputs[idx].ncomp = ncomp;

   ir->num_inputs = MAX2(ir->num_inputs, idx + 1);
}

static inline void
eir_assign_output(struct eir *ir, unsigned idx, unsigned slot, unsigned ncomp)
{
   assert(ir);
   assert(idx < ARRAY_SIZE(ir->outputs));

   ir->outputs[idx].reg = idx;
   ir->outputs[idx].slot = slot;
   ir->outputs[idx].ncomp = ncomp;

   ir->num_outputs = MAX2(ir->num_outputs, idx + 1);
}

void
eir_legalize(struct eir *ir);

void
eir_calculate_live_intervals(struct eir *ir);

int
eir_temp_range_start(const struct eir* ir, unsigned n);

int
eir_temp_range_end(const struct eir* ir, unsigned n);

struct eir_ra_reg_set *
eir_ra_alloc_reg_set(void *memctx);

uint32_t *
eir_assemble(const struct eir *ir, struct eir_info *info);

static inline struct eir_instruction *
eir_SET(struct eir_block *block, struct eir_register *dst,
        struct eir_register *src0, enum gc_cond condition,
        struct eir_register *src1)
{
   struct eir_instruction *instr;

   assert(gc_op_has_dst(GC_SET));
   assert(gc_op_num_src(GC_SET) == 2);

   instr = eir_instruction_create(block, GC_SET);
   instr->gc.type = GC_OP_TYPE_ALU;
   instr->gc.condition = condition;
   instr->dst = *dst;
   instr->src[0] = *src0;
   instr->src[1] = *src1;

   return instr;
}

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

static inline struct eir_instruction *
eir_TEXKILL_IF(struct eir_block *block, struct eir_register *src0,
               enum gc_cond condition, struct eir_register *src1)
{
   struct eir_instruction *instr;

   assert(!gc_op_has_dst(GC_TEXKILL_IF));
   assert(gc_op_num_src(GC_TEXKILL_IF) == 2);

   instr = eir_instruction_create(block, GC_TEXKILL_IF);
   instr->gc.type = GC_OP_TYPE_ALU;
   instr->gc.condition = condition;
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

#define eir_for_each_inst_safe(inst, block)                                  \
   list_for_each_entry_safe(struct eir_instruction, inst, &block->instr_list, node)

#ifdef __cplusplus
}
#endif

#endif // H_EIR
