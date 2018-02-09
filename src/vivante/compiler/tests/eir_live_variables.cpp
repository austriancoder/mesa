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

TEST (LiveVariableTest, NOP)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   eir_NOP(block);

   eir_calculate_live_intervals(ir);

   ASSERT_EQ(ir->temp_start[0], ~0);
   ASSERT_EQ(ir->temp_end[0], -1);

   eir_destroy(ir);
}

TEST (LiveVariableTest, MOV)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   struct gc_instr_dst dst = {
      .reg = 0,
   };
   struct gc_instr_src src0 = {
      .reg = 1,
   };
   struct gc_instr_src src1 = {
      .reg = 2,
   };

   eir_MOV(block, &dst, &src0);
   eir_NOP(block);
   eir_MOV(block, &dst, &src1);

   eir_calculate_live_intervals(ir);

   ASSERT_EQ(ir->temp_start[0], -1);
   ASSERT_EQ(ir->temp_end[0], 2);

   ASSERT_EQ(ir->temp_start[1], -1);
   ASSERT_EQ(ir->temp_end[1], 0);

   ASSERT_EQ(ir->temp_start[2], -1);
   ASSERT_EQ(ir->temp_end[2], 2);

   eir_destroy(ir);
}
