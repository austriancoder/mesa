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

TEST (LegalizeTest, OneDifferenUniform)
{
   struct eir *ir = eir_create();
   ASSERT_TRUE(ir);

   struct eir_block *block = eir_block_create(ir);
   ASSERT_TRUE(block);

   struct gc_instr_dst d = {};
   struct gc_instr_src s0 = {
      .reg = 0,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };
   struct gc_instr_src s1 = {
      .reg = 1,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };

   eir_ADD(block, &d, &s0, &s1);

   eir_legalize(ir);
   ASSERT_EQ(list_length(&block->instr_list), 2);

   eir_dump(ir);

   eir_destroy(ir);
}

TEST (LegalizeTest, DifferenUniforms)
{
   struct eir *ir = eir_create();
   ASSERT_TRUE(ir);

   struct eir_block *block = eir_block_create(ir);
   ASSERT_TRUE(block);

   struct gc_instr_dst d = {};
   struct gc_instr_src s0 = {
      .reg = 0,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };
   struct gc_instr_src s1 = {
      .reg = 1,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };
   struct gc_instr_src s2 = {
      .reg = 2,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };

   eir_MAD(block, &d, &s0, &s1, &s2);

   eir_legalize(ir);
   ASSERT_EQ(list_length(&block->instr_list), 3);

   eir_dump(ir);

   eir_destroy(ir);
}

TEST (LegalizeTest, SameUniforms)
{
   struct eir *ir = eir_create();
   ASSERT_TRUE(ir);

   struct eir_block *block = eir_block_create(ir);
   ASSERT_TRUE(block);

   struct gc_instr_dst d = {};
   struct gc_instr_src s0 = {
      .reg = 1,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };
   struct gc_instr_src s1 = {
      .reg = 1,
      .rgroup = GC_REGISTER_GROUP_UNIFORM
   };

   eir_ADD(block, &d, &s0, &s1);

   eir_legalize(ir);
   ASSERT_EQ(list_length(&block->instr_list), 1);

   eir_destroy(ir);
}
