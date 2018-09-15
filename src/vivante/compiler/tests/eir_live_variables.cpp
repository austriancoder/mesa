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
#include <limits.h>
#include "vivante/compiler/eir.h"

TEST (LiveVariableTest, NOP)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   eir_NOP(block);

   eir_calculate_live_intervals(ir);

   ASSERT_FALSE(ir->temp_start);
   ASSERT_FALSE(ir->temp_end);

   ASSERT_EQ(eir_temp_range_start(ir, 0), INT_MAX);
   ASSERT_EQ(eir_temp_range_end(ir, 0), INT_MIN);

   eir_destroy(ir);
}

TEST (LiveVariableTest, MOV)
{
   struct eir *ir = eir_create();
   struct eir_block *block = eir_block_create(ir);

   struct eir_register dst = eir_temp_register(ir, 4);
   dst.writemask = INST_COMPS_X;

   struct eir_register src0 = eir_temp_register(ir, 4);
   src0.swizzle = INST_SWIZ_BROADCAST(0);

   struct eir_register src1 = eir_temp_register(ir, 4);
   src1.swizzle = INST_SWIZ_BROADCAST(1);

   eir_MOV(block, &dst, &src0);
   eir_NOP(block);
   eir_MOV(block, &dst, &src1);

   eir_calculate_live_intervals(ir);

   static const int MAX_INSTRUCTION = (1 << 30);

   // dst x___
   ASSERT_EQ(ir->temp_start[0], 0);
   ASSERT_EQ(ir->temp_start[1], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_start[2], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_start[3], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_end[0], 2);
   ASSERT_EQ(ir->temp_end[1], -1);
   ASSERT_EQ(ir->temp_end[2], -1);
   ASSERT_EQ(ir->temp_end[3], -1);

   ASSERT_EQ(eir_temp_range_start(ir, 0), 0);
   ASSERT_EQ(eir_temp_range_end(ir, 0), 2);

   // src0 xxxx
   ASSERT_EQ(ir->temp_start[4], 0);
   ASSERT_EQ(ir->temp_start[5], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_start[6], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_start[7], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_end[4], 0);
   ASSERT_EQ(ir->temp_end[5], -1);
   ASSERT_EQ(ir->temp_end[6], -1);
   ASSERT_EQ(ir->temp_end[7], -1);

   ASSERT_EQ(eir_temp_range_start(ir, 1), 0);
   ASSERT_EQ(eir_temp_range_end(ir, 1), 0);

   // src1 yyyy
   ASSERT_EQ(ir->temp_start[8], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_start[9], 0);
   ASSERT_EQ(ir->temp_start[10], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_start[11], MAX_INSTRUCTION);
   ASSERT_EQ(ir->temp_end[8], -1);
   ASSERT_EQ(ir->temp_end[9], 2);
   ASSERT_EQ(ir->temp_end[10], -1);
   ASSERT_EQ(ir->temp_end[11], -1);

   ASSERT_EQ(eir_temp_range_start(ir, 2), 0);
   ASSERT_EQ(eir_temp_range_end(ir, 2), 2);

   eir_destroy(ir);
}
