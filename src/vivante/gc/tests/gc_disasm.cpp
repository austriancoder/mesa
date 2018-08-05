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
#include "util/ralloc.h"

struct disasm_state
{
   uint32_t bin[4];
   const char *disasm;

   friend std::ostream& operator<<(std::ostream &os, const disasm_state &obj)
   {
      return os
         << "binary: "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[0] << " "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[1] << " "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[2] << " "
         << std::showbase << std::internal << std::setfill('0') << std::setw(8) << std::hex << obj.bin[3] << " "
         << "disasm: " << obj.disasm;
   }
};

struct DisasmTest : testing::Test, testing::WithParamInterface<disasm_state>
{
   struct gc_string decoded;

   DisasmTest()
   {
      decoded.string = (char *)rzalloc_size(NULL, 1);
      decoded.offset = 0;

      gc_disasm(&decoded, GetParam().bin);
   }
};

TEST_P(DisasmTest, basicOpCodes)
{
   auto as = GetParam();
   EXPECT_STREQ(as.disasm, decoded.string);

   ralloc_free(decoded.string);
}

INSTANTIATE_TEST_CASE_P(Default, DisasmTest,
   testing::Values(
      disasm_state{ {0x00000000, 0x00000000, 0x00000000, 0x00000000}, "nop        void, void, void, void" },
      disasm_state{ {0x07811018, 0x15001f20, 0x00000000, 0x00000000}, "texld      t1, tex0, t1.xyyy, void, void" },
      disasm_state{ {0x07831018, 0x39003f20, 0x00000000, 0x00000000}, "texld      t3, tex0, t3, void, void" },
      disasm_state{ {0x07811009, 0x00000000, 0x00000000, 0x20390008}, "mov        t1, void, void, u0" },
      disasm_state{ {0x01821009, 0x00000000, 0x00000000, 0x00150028}, "mov        t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00000000, 0x00000000, 0x00550028}, "mov        t2.xy__, void, void, -t2.xyyy"},
      disasm_state{ {0x01821009, 0x00000000, 0x00000000, 0x00950028}, "mov        t2.xy__, void, void, |t2.xyyy|"}
   )
);

INSTANTIATE_TEST_CASE_P(OperandTypes, DisasmTest,
   testing::Values(
      disasm_state{ {0x01821009, 0x00000000, 0x00000000, 0x00150028}, "mov        t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00000000, 0x40000000, 0x00150028}, "mov.s32    t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00000000, 0x80000000, 0x00150028}, "mov.s8     t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00000000, 0xc0000000, 0x00150028}, "mov.u16    t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00200000, 0x00000000, 0x00150028}, "mov.f16    t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00200000, 0x40000000, 0x00150028}, "mov.s16    t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00200000, 0x80000000, 0x00150028}, "mov.u32    t2.xy__, void, void, t2.xyyy"},
      disasm_state{ {0x01821009, 0x00200000, 0xc0000000, 0x00150028}, "mov.u8     t2.xy__, void, void, t2.xyyy"}
   )
);
