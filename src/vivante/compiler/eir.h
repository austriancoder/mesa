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
#include "util/bitset.h"
#include "util/list.h"
#include "vivante/gc/gc_instr.h"

#ifdef __cplusplus
extern "C"{
#endif

struct eir;
struct eir_block;
struct eir_compiler;
struct eir_ra_reg_set;

struct eir_instruction
{
   struct eir_block *block;
   struct list_head node;  /* entry in eir_block's instruction list */

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

struct eir
{
   struct list_head block_list;

   unsigned blocks;
   unsigned reg_alloc;

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

struct gc_instr *
eir_instruction_create(struct eir_block *block, enum gc_op opc);

void
eir_link_blocks(struct eir_block *predecessor, struct eir_block *successor);

void
eir_calculate_live_intervals(struct eir *ir);

int
eir_temp_range_start(const struct eir* ir, unsigned n);

int
eir_temp_range_end(const struct eir* ir, unsigned n);

struct eir_ra_reg_set *
eir_ra_alloc_reg_set(void *memctx);

bool
eir_register_allocate(struct eir *ir, struct eir_compiler *compiler);

uint32_t *
eir_assemble(struct eir *ir, struct eir_info *info);

static inline struct gc_instr *
eir_BRANCH(struct eir_block *block, struct gc_instr_src *src0,
           struct gc_instr_src *src1)
{
   struct gc_instr *instr;
   struct gc_instr_src *src[] = { src0, src1 };

   assert(!gc_op_has_dst(GC_BRANCH));
   assert(gc_op_num_src(GC_BRANCH) == 2);

   instr = eir_instruction_create(block, GC_BRANCH);
   instr->type = GC_OP_TYPE_BRANCH;
   instr->branch.imm = ~0;

   const unsigned swizzle = gc_op_src_swizzle(GC_BRANCH);

   for (int i = 0; i < 2; i++) {
      const unsigned shift = i * 2;
      const unsigned mask = 0x3 << shift;
      const unsigned index = (swizzle & mask) >> shift;

      if (index == 0x3)
         continue;

      assert(src[i]);
      instr->alu.src[index] = *src[i];
      instr->alu.src[index].use = true;
   }

   return instr;
}

#define ALU0(opc) \
static inline struct gc_instr * \
eir_##opc(struct eir_block *block) \
{ \
   struct gc_instr *instr; \
   \
   assert(!gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 0);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->type = GC_OP_TYPE_ALU; \
   \
   return instr; \
}

#define ALU1(opc) \
static inline struct gc_instr * \
eir_##opc(struct eir_block *block, struct gc_instr_dst *dst, \
          struct gc_instr_src *src0) \
{ \
   struct gc_instr *instr; \
   struct gc_instr_src *src[] = { src0 }; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) >= 1);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->type = GC_OP_TYPE_ALU; \
   instr->dst = *dst; \
   \
   const unsigned swizzle = gc_op_src_swizzle(GC_##opc); \
   \
   for (int i = 0; i < 1; i++) { \
      const unsigned shift = i * 2; \
      const unsigned mask = 0x3 << shift; \
      const unsigned index = (swizzle & mask) >> shift; \
      \
      if (index == 0x3) \
         continue; \
      \
      assert(src[i]); \
      instr->alu.src[index] = *src[i]; \
      instr->alu.src[index].use = true; \
   } \
   \
   /* special handling for src replication */ \
   if (swizzle & SWIZ_REP_SRC0_TO_SRC2) \
   { \
      instr->alu.src[2] = *src[0]; \
      instr->alu.src[2].use = true; \
   } \
   \
   return instr; \
}

#define ALU2(opc) \
static inline struct gc_instr * \
eir_##opc(struct eir_block *block, struct gc_instr_dst *dst, \
          struct gc_instr_src *src0, struct gc_instr_src *src1) \
{ \
   struct gc_instr *instr; \
   struct gc_instr_src *src[] = { src0, src1 }; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 2);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->type = GC_OP_TYPE_ALU; \
   instr->dst = *dst; \
   \
   const unsigned swizzle = gc_op_src_swizzle(GC_##opc); \
   \
   for (int i = 0; i < 2; i++) { \
      const unsigned shift = i * 2; \
      const unsigned mask = 0x3 << shift; \
      const unsigned index = (swizzle & mask) >> shift; \
      \
      if (index == 0x3) \
         continue; \
      \
      assert(src[i]); \
      instr->alu.src[index] = *src[i]; \
      instr->alu.src[index].use = true; \
   } \
   \
   return instr; \
}

#define ALU3(opc) \
static inline struct gc_instr * \
eir_##opc(struct eir_block *block, struct gc_instr_dst *dst, \
          struct gc_instr_src *src0, struct gc_instr_src *src1, \
          struct gc_instr_src *src2) \
{ \
   struct gc_instr *instr; \
   struct gc_instr_src *src[] = { src0, src1, src2 }; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 3);\
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->type = GC_OP_TYPE_ALU; \
   instr->dst = *dst; \
   \
   const unsigned swizzle = gc_op_src_swizzle(GC_##opc); \
   \
   for (int i = 0; i < 3; i++) { \
      const unsigned shift = i * 2; \
      const unsigned mask = 0x3 << shift; \
      const unsigned index = (swizzle & mask) >> shift; \
      \
      if (index == 0x3) \
         continue; \
      \
      assert(src[index]); \
      instr->alu.src[i] = *src[index]; \
      instr->alu.src[i].use = true; \
   } \
   \
   return instr; \
}

#define TEX(opc) \
static inline struct gc_instr * \
eir_##opc(struct eir_block *block, struct gc_instr_dst *dst, \
          struct gc_instr_src *src) \
{ \
   struct gc_instr *instr; \
   \
   assert(gc_op_has_dst(GC_##opc));\
   assert(gc_op_num_src(GC_##opc) == 1); \
   \
   instr = eir_instruction_create(block, GC_##opc); \
   instr->type = GC_OP_TYPE_SAMPLER; \
   instr->dst = *dst; \
   instr->sampler.src = *src; \
   instr->sampler.src.use = true; \
   \
   return instr; \
}

ALU0(NOP);

ALU1(MOV);
ALU1(RCP);
ALU1(RSQ);
ALU1(CEIL);
ALU1(FLOOR);
ALU1(FRC);
ALU1(LOG);
ALU1(SQRT);
ALU1(EXP);
ALU1(DSX);
ALU1(DSY);

ALU2(ADD);
ALU2(MUL);
ALU2(DP2);
ALU2(DP3);
ALU2(DP4);
ALU2(SET);

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
