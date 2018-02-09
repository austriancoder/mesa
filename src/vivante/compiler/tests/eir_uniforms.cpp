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

TEST (UniformTest, UniqueValues)
{
   struct eir *ir = eir_create();

   ASSERT_TRUE(ir);

   eir_uniform_u32(ir, 0);
   eir_uniform_u32(ir, 1);
   eir_uniform_u32(ir, 2);

   struct eir_uniform_info *info;
   info = eir_uniform_info(ir);

   ASSERT_EQ(3, info->count);

   eir_destroy(ir);
}

TEST (UniformTest, SameValues)
{
   struct eir *ir = eir_create();

   ASSERT_TRUE(ir);

   eir_uniform_u32(ir, 0);
   eir_uniform_u32(ir, 0);
   eir_uniform_u32(ir, 0);

   struct eir_uniform_info *info;
   info = eir_uniform_info(ir);

   ASSERT_EQ(1, info->count);

   eir_destroy(ir);
}

TEST (UniformTest, UniqueVec4Values)
{
   struct eir *ir = eir_create();
   uint32_t values[4] = { 1, 2, 3, 4 };

   ASSERT_TRUE(ir);

   eir_uniform_vec4_u32(ir, values);

   struct eir_uniform_info *info;
   info = eir_uniform_info(ir);

   ASSERT_EQ(4, info->count);

   eir_destroy(ir);
}

TEST (UniformTest, SameVec4Values)
{
   struct eir *ir = eir_create();
   uint32_t values[4] = { 1, 2, 3, 4 };
   struct gc_instr_src s;

   ASSERT_TRUE(ir);

   s = eir_uniform_vec4_u32(ir, values);
   ASSERT_EQ(s.reg, 0);

   s = eir_uniform_vec4_u32(ir, values);
   ASSERT_EQ(s.reg, 0);

   s = eir_uniform_vec4_u32(ir, values);
   ASSERT_EQ(s.reg, 0);

   struct eir_uniform_info *info;
   info = eir_uniform_info(ir);

   ASSERT_EQ(4, info->count);

   eir_destroy(ir);
}
