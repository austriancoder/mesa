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
#include "etnaviv/compiler/eir.h"

TEST (UniformTest, Basic)
{
   struct eir *ir = eir_create();
   struct eir_register u;
   const struct eir_uniform_data *data;

   u = eir_uniform_register_ui(ir, 1);

   ASSERT_EQ(u.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u.index, 0);
   ASSERT_EQ(u.swizzle, INST_SWIZ_BROADCAST(0));

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 0);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 1);

   eir_destroy(ir);
}

TEST (UniformTest, ReuseUnused)
{
   struct eir *ir = eir_create();
   struct eir_register u1, u2;

   eir_uniform_register(ir, EIR_UNIFORM_UNUSED, 0);

   ASSERT_EQ(util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data), 1);

   u1 = eir_uniform_register_ui(ir, 1);
   u2 = eir_uniform_register_ui(ir, 1);

   ASSERT_EQ(util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data), 1);

   ASSERT_EQ(u1.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u1.index, 0);
   ASSERT_EQ(u1.swizzle, INST_SWIZ_BROADCAST(0));

   ASSERT_EQ(u2.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u2.index, 0);
   ASSERT_EQ(u2.swizzle, INST_SWIZ_BROADCAST(0));

   eir_destroy(ir);
}

TEST (UniformTest, ReuseUnusedCausedByVec4)
{
   struct eir *ir = eir_create();
   struct eir_register u1, u2, u3;

   u1 = eir_uniform_register_ui(ir, 0);

   ASSERT_EQ(util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data), 1);

   uint32_t v[] = {
      10,
      11,
      12,
      13,
   };

   u2 = eir_uniform_register_vec4_ui(ir, 4, v);

   ASSERT_EQ(util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data), 8);

   u3 = eir_uniform_register_ui(ir, 1);

   ASSERT_EQ(util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data), 8);

   ASSERT_EQ(u1.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u1.index, 0);
   ASSERT_EQ(u1.swizzle, INST_SWIZ_BROADCAST(0));

   ASSERT_EQ(u2.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u2.index, 1);
   ASSERT_EQ(u2.swizzle, INST_SWIZ_IDENTITY);

   ASSERT_EQ(u3.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u3.index, 0);
   ASSERT_EQ(u3.swizzle, INST_SWIZ_BROADCAST(1));

   eir_destroy(ir);
}

TEST (UniformTest, EqualUniforms)
{
   struct eir *ir = eir_create();
   struct eir_register u1, u2;

   u1 = eir_uniform_register_ui(ir, 1);
   u2 = eir_uniform_register_ui(ir, 1);

   ASSERT_EQ(u1.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u1.index, 0);
   ASSERT_EQ(u1.swizzle, INST_SWIZ_BROADCAST(0));

   ASSERT_EQ(u2.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u2.index, 0);
   ASSERT_EQ(u2.swizzle, INST_SWIZ_BROADCAST(0));

   eir_destroy(ir);
}

TEST (UniformTest, MultipeSwizzles)
{
   struct eir *ir = eir_create();
   struct eir_register u1, u2, u3, u4, u5;
   const struct eir_uniform_data *data;

   u1 = eir_uniform_register_ui(ir, 0);
   u2 = eir_uniform_register_ui(ir, 1);
   u3 = eir_uniform_register_ui(ir, 2);
   u4 = eir_uniform_register_ui(ir, 3);
   u5 = eir_uniform_register_ui(ir, 4);

   ASSERT_EQ(u1.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u1.index, 0);
   ASSERT_EQ(u1.swizzle, INST_SWIZ_BROADCAST(0));

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 0);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 0);


   ASSERT_EQ(u2.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u2.index, 0);
   ASSERT_EQ(u2.swizzle, INST_SWIZ_BROADCAST(1));

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 1);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 1);


   ASSERT_EQ(u3.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u3.index, 0);
   ASSERT_EQ(u3.swizzle, INST_SWIZ_BROADCAST(2));

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 2);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 2);

   ASSERT_EQ(u4.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u4.index, 0);
   ASSERT_EQ(u4.swizzle, INST_SWIZ_BROADCAST(3));

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 3);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 3);


   ASSERT_EQ(u5.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u5.index, 1);
   ASSERT_EQ(u5.swizzle, INST_SWIZ_BROADCAST(0));

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 4);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 4);

   eir_destroy(ir);
}

TEST (UniformTest, BasicVec4)
{
   static const uint32_t values[4] = { 1, 2, 3, 4 };
   struct eir *ir = eir_create();
   struct eir_register u;
   struct eir_uniform_data *data;

   u = eir_uniform_register_vec4_ui(ir, 4, values);

   ASSERT_EQ(u.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u.index, 0);
   ASSERT_EQ(u.swizzle, INST_SWIZ_IDENTITY);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 0);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 1);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 1);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 2);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 2);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 3);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 3);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 4);

   eir_destroy(ir);
}

TEST (UniformTest, EqualUnfiormsVec4)
{
   static const uint32_t values[4] = { 1, 2, 3, 4 };
   struct eir *ir = eir_create();
   struct eir_register u1, u2;
   struct eir_uniform_data *data;

   u1 = eir_uniform_register_vec4_ui(ir, 4, values);
   u2 = eir_uniform_register_vec4_ui(ir, 4, values);

   ASSERT_EQ(u1.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u1.index, 0);
   ASSERT_EQ(u1.swizzle, INST_SWIZ_IDENTITY);

   ASSERT_EQ(u2.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u2.index, 0);
   ASSERT_EQ(u2.swizzle, INST_SWIZ_IDENTITY);


   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 0);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 1);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 1);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 2);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 2);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 3);

   data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, 3);
   ASSERT_EQ(data->content, EIR_UNIFORM_CONSTANT);
   ASSERT_EQ(data->data, 4);

   eir_destroy(ir);
}

TEST (UniformTest, ncomp1)
{
   struct eir *ir = eir_create();

   static const eir_uniform_contents contents[4] = {
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_UNUSED,
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
   };

   static const uint32_t values[4] = {
      1,
      2,
      3,
      4,
   };

   eir_uniform_register_vec4(ir, 4, contents, values);

   struct eir_register u = eir_uniform_register_ui(ir, 2);

   ASSERT_EQ(u.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u.index, 0);
   ASSERT_EQ(u.swizzle, INST_SWIZ_BROADCAST(1));
}

TEST (UniformTest, ncomp2)
{
   struct eir *ir = eir_create();

   static const eir_uniform_contents contents[] = {
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_UNUSED,
      EIR_UNIFORM_UNUSED,
      EIR_UNIFORM_CONSTANT,
   };

   static const uint32_t values[] = {
      1,
      2,
      3,
      4,
   };

   eir_uniform_register_vec4(ir, 4, contents, values);


   static const eir_uniform_contents c[] = {
      EIR_UNIFORM_CONSTANT,
      EIR_UNIFORM_CONSTANT,
   };

   static const uint32_t v[] = {
      10,
      10,
   };

   struct eir_register u = eir_uniform_register_vec4(ir, 2, c, v);

   ASSERT_EQ(u.type, eir_register::EIR_REG_UNIFORM);
   ASSERT_EQ(u.index, 0);
   ASSERT_EQ(u.swizzle, INST_SWIZ_BROADCAST(1));
}
