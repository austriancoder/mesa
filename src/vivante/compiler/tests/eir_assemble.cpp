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

#include <gtest/gtest.h>
#include "vivante/compiler/eir.h"

TEST (AssembleTest, NopAssembleDisassemble)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);
 
   eir_NOP(block);

   struct eir_info info;
   void *bin = eir_assemble(ir, &info);
   ASSERT_TRUE(bin);

   eir_destroy(ir);

   struct gc_instr instr;
   gc_unpack_instr((uint32_t* )bin, &instr);

   ASSERT_EQ(instr.type, GC_OP_TYPE_ALU);
   ASSERT_EQ(instr.opcode, GC_NOP);

   ASSERT_EQ(info.sizedwords, 4);

   free(bin);
}

TEST (AssembleTest, Swizzle)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   struct eir_register d = eir_temp_register(ir, 4);
   struct eir_register s0 = eir_temp_register(ir, 4);
   struct eir_register s1 = eir_temp_register(ir, 4);

   eir_ADD(block, &d, &s0, &s1);

   struct eir_info info;
   void *bin = eir_assemble(ir, &info);
   ASSERT_TRUE(bin);

   eir_destroy(ir);

   struct gc_instr instr;
   gc_unpack_instr((uint32_t* )bin, &instr);

   ASSERT_EQ(instr.type, GC_OP_TYPE_ALU);
   ASSERT_EQ(instr.opcode, GC_ADD);
   ASSERT_TRUE(instr.alu.src[0].use);
   ASSERT_FALSE(instr.alu.src[1].use);
   ASSERT_TRUE(instr.alu.src[2].use);
   ASSERT_EQ(instr.alu.src[0].reg, 1);
   ASSERT_EQ(instr.alu.src[2].reg, 2);
   ASSERT_EQ(instr.dst.reg, 0);

   free(bin);
}

TEST (AssembleTest, SwizzleReplicate)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   struct eir_register d = eir_temp_register(ir, 4);
   struct eir_register s = eir_temp_register(ir, 4);
   eir_DSX(block, &d, &s);

   struct eir_info info;
   void *bin = eir_assemble(ir, &info);
   ASSERT_TRUE(bin);

   eir_destroy(ir);

   struct gc_instr instr;
   gc_unpack_instr((uint32_t* )bin, &instr);

   ASSERT_EQ(instr.type, GC_OP_TYPE_ALU);
   ASSERT_EQ(instr.opcode, GC_DSX);
   ASSERT_TRUE(instr.alu.src[0].use);
   ASSERT_FALSE(instr.alu.src[1].use);
   ASSERT_TRUE(instr.alu.src[2].use);
   ASSERT_EQ(instr.alu.src[0].reg, 1);
   ASSERT_EQ(instr.alu.src[2].reg, 1);

   free(bin);
}
