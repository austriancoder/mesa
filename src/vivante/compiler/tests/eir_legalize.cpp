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

#include <gtest/gtest.h>
#include "vivante/compiler/eir.h"

TEST (LegalizeTest, EmptyShader)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   eir_legalize(ir);

   ASSERT_EQ(list_length(&block->instr_list), 1);
   struct eir_instruction *nop = list_first_entry(&block->instr_list, struct eir_instruction, node);
   ASSERT_EQ(nop->gc.opcode, GC_NOP);

   eir_destroy(ir);
}

TEST (LegalizeTest, UniformsTwoSame)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);
   struct eir_register dst = eir_temp_register(ir, 4);
   struct eir_register src = eir_uniform_register_ui(ir, 1);

   dst.writemask = 0xf;

   eir_ADD(block, &dst, &src, &src);

   eir_legalize(ir);

   ASSERT_EQ(list_length(&block->instr_list), 1);
   struct eir_instruction *add = list_first_entry(&block->instr_list, struct eir_instruction, node);
   ASSERT_EQ(add->gc.opcode, GC_ADD);

   eir_destroy(ir);
}

TEST (LegalizeTest, UniformsThreeSame)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);
   struct eir_register dst = eir_temp_register(ir, 4);
   struct eir_register src = eir_uniform_register_ui(ir, 1);

   dst.writemask = 0xf;

   eir_MAD(block, &dst, &src, &src, &src);

   eir_legalize(ir);

   ASSERT_EQ(list_length(&block->instr_list), 1);
   struct eir_instruction *mad = list_first_entry(&block->instr_list, struct eir_instruction, node);
   ASSERT_EQ(mad->gc.opcode, GC_MAD);

   eir_destroy(ir);
}


TEST (LegalizeTest, UniformsTwoDifferent)
{
   static const uint32_t val0[] = { 0, 1, 2, 3 };
   static const uint32_t val1[] = { 4, 5, 6, 7 };
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);
   struct eir_register dst = eir_temp_register(ir, 4);
   struct eir_register src0 = eir_uniform_register_vec4_ui(ir, val0);
   struct eir_register src1 = eir_uniform_register_vec4_ui(ir, val1);

   dst.writemask = 0xf;

   eir_ADD(block, &dst, &src0, &src1);

   eir_legalize(ir);

   ASSERT_EQ(list_length(&block->instr_list), 2);
   struct eir_instruction *mov = list_first_entry(&block->instr_list, struct eir_instruction, node);
   ASSERT_EQ(mov->gc.opcode, GC_MOV);
   ASSERT_EQ(mov->src[0].index, 1);
   ASSERT_EQ(mov->src[0].type, eir_register::EIR_REG_UNIFORM);

   eir_destroy(ir);
}

TEST (LegalizeTest, UniformsThreeDifferent)
{
   static const uint32_t val0[] = { 0, 1, 2, 3 };
   static const uint32_t val1[] = { 4, 5, 6, 7 };
   static const uint32_t val2[] = { 8, 9, 10, 11 };
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);
   struct eir_register dst = eir_temp_register(ir, 4);
   struct eir_register src0 = eir_uniform_register_vec4_ui(ir, val0);
   struct eir_register src1 = eir_uniform_register_vec4_ui(ir, val1);
   struct eir_register src2 = eir_uniform_register_vec4_ui(ir, val2);

   dst.writemask = 0xf;

   eir_MAD(block, &dst, &src0, &src1, &src2);

   eir_legalize(ir);

   ASSERT_EQ(list_length(&block->instr_list), 3);
   struct eir_instruction *mov = list_first_entry(&block->instr_list, struct eir_instruction, node);
   ASSERT_EQ(mov->gc.opcode, GC_MOV);
   ASSERT_EQ(mov->src[0].index, 1);
   ASSERT_EQ(mov->src[0].type, eir_register::EIR_REG_UNIFORM);

   eir_destroy(ir);
}
