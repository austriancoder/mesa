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
#include "vivante/gc/gc_instr.h"

struct instr_state
{
   enum gc_op op;
   enum gc_op_type type;
   bool dst;
   int num;

   friend std::ostream& operator<<(std::ostream &os, const instr_state &obj)
   {
      return os
         << "opc: "  << obj.op   << " "
         << "type: " << obj.type << " "
         << "dst: "  << obj.dst  << " "
         << "num: "  << obj.num  << " ";
   }
};

struct InstrTest : testing::Test, testing::WithParamInterface<instr_state>
{
   enum gc_op_type type;
   bool dst;
   int num;

   InstrTest()
   {
      type = gc_op_type(GetParam().op);
      dst = gc_op_has_dst(GetParam().op);
      num = gc_op_num_src(GetParam().op);
   }
};

TEST_P(InstrTest, gc_op_xxx)
{
   auto as = GetParam();
   EXPECT_EQ(as.type, type);
   EXPECT_EQ(as.dst, dst);
   EXPECT_EQ(as.num, num);
}

INSTANTIATE_TEST_CASE_P(Default, InstrTest,
   testing::Values(
      instr_state{ GC_NOP, GC_OP_TYPE_ALU, false, 0 },
      instr_state{ GC_RCP, GC_OP_TYPE_ALU, true,  1 },
      instr_state{ GC_ADD, GC_OP_TYPE_ALU, true,  2 },
      instr_state{ GC_MAD, GC_OP_TYPE_ALU, true,  3 }
   )
);
