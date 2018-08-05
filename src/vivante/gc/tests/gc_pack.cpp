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
#include "vivante/gc/gc_disasm.h"
#include "vivante/gc/gc_instr.h"

#include "util/ralloc.h"

struct instr_state
{
   struct gc_instr instr;
   uint32_t bin[4];

   friend std::ostream& operator<<(std::ostream &os, const instr_state &obj)
   {
      const char *decoded = gc_dump_string(&obj.instr);

      os
         << "instr:  " << decoded    << " "
         << "binary: "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[0] << " "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[1] << " "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[2] << " "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[3];

      ralloc_free((void *)decoded);

      return os;
   }
};

struct InstrPackTest : testing::Test, testing::WithParamInterface<instr_state>
{
   uint32_t bin[4];

   InstrPackTest()
   {
      memset(&bin, 0, sizeof(bin));

      gc_pack_instr(&GetParam().instr, (uint32_t *)&bin);
   }
};

TEST_P(InstrPackTest, pack)
{
   auto as = GetParam();
   EXPECT_EQ(as.bin[0], bin[0]);
   EXPECT_EQ(as.bin[1], bin[1]);
   EXPECT_EQ(as.bin[2], bin[2]);
   EXPECT_EQ(as.bin[3], bin[3]);
}

INSTANTIATE_TEST_CASE_P(Default, InstrPackTest,
   testing::Values(
      instr_state{
         .instr = {
            .type = GC_OP_TYPE_ALU,
            .opcode = GC_NOP
         },
         {0x00000000, 0x00000000, 0x00000000, 0x00000000}
      },
      instr_state{
         .instr = {
            .type = GC_OP_TYPE_ALU,
            .opcode = GC_ADD,
         },
         {0x00001001, 0x00000000, 0x00000000, 0x00000000}
      },
      instr_state{
         .instr = {
            .type = GC_OP_TYPE_ALU,
            .opcode = GC_IMULHI1,
         },
         {0x00001001, 0x00000000, 0x000010000, 0x00000000}
      }
   )
);
