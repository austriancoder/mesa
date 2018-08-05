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

#include "gc_disasm.h"
#include "gc_instr.h"

#include "isa.xml.h"

#include <stdio.h>

#include "util/macros.h"

static void
gc_decode_operand_type(struct gc_string *string,
                       const enum gc_operand_type type)
{
   switch (type) {
   case GC_OPERAND_F32:
      /* nothing to output */
      break;

   case GC_OPERAND_S32:
      gc_string_append(string, ".s32");
      break;

   case GC_OPERAND_S8:
      gc_string_append(string, ".s8");
      break;

   case GC_OPERAND_U16:
      gc_string_append(string, ".u16");
      break;

   case GC_OPERAND_F16:
      gc_string_append(string, ".f16");
      break;

   case GC_OPERAND_S16:
      gc_string_append(string, ".s16");
      break;

   case GC_OPERAND_U32:
      gc_string_append(string, ".u32");
      break;

   case GC_OPERAND_U8:
      gc_string_append(string, ".u8");
      break;
   }
}

static void
gc_decode_address_mode(struct gc_string *string,
                       const enum gc_addressing_mode mode)
{
   switch (mode) {
   case GC_ADDRESSING_MODE_DIRECT:
      /* nothing to output */
      break;

   case GC_ADDRESSING_MODE_ADD_A_X:
      gc_string_append(string, "[a.x]");
      break;

   case GC_ADDRESSING_MODE_ADD_A_Y:
      gc_string_append(string, "[a.y]");
      break;

   case GC_ADDRESSING_MODE_ADD_A_Z:
      gc_string_append(string, "[a.z]");
      break;

   case GC_ADDRESSING_MODE_ADD_A_W:
      gc_string_append(string, "[a.w]");
      break;
   }
}

static void
gc_decode_gc_register_group(struct gc_string *string,
                            const enum gc_register_group rgoup)
{
   switch (rgoup) {
   case GC_REGISTER_GROUP_INTERNAL:
      gc_string_append(string, "i");
      break;

   case GC_REGISTER_GROUP_TEMP:
      gc_string_append(string, "t");
      break;

   case GC_REGISTER_GROUP_UNIFORM:
      gc_string_append(string, "u");
      break;
   }
}

void
gc_decode_components(struct gc_string *string, const uint8_t components)
{
   if (components == 15)
      return;

   gc_string_append(string, ".");
   if (components & INST_COMPS_X)
      gc_string_append(string, "x");
   else
      gc_string_append(string, "_");

   if (components & INST_COMPS_Y)
      gc_string_append(string, "y");
   else
      gc_string_append(string, "_");

   if (components & INST_COMPS_Z)
      gc_string_append(string, "z");
   else
      gc_string_append(string, "_");

   if (components & INST_COMPS_W)
      gc_string_append(string, "w");
   else
      gc_string_append(string, "_");
}

static void
gc_decode_swiz_comp(struct gc_string *string, const uint8_t swiz_comp)
{
   switch (swiz_comp) {
   case INST_SWIZ_COMP_X:
      gc_string_append(string, "x");
      break;

   case INST_SWIZ_COMP_Y:
      gc_string_append(string, "y");
      break;

   case INST_SWIZ_COMP_Z:
      gc_string_append(string, "z");
      break;

   case INST_SWIZ_COMP_W:
      gc_string_append(string, "w");
      break;

   default:
      unreachable("unsupported swizling");
      break;
   }
}

void
gc_decode_swiz(struct gc_string *string, const uint8_t swiz)
{
   /* if a null swizzle */
   if (swiz == 0xe4)
      return;

   const unsigned x = swiz & 0x3;
   const unsigned y = (swiz & 0x0C) >> 2;
   const unsigned z = (swiz & 0x30) >> 4;
   const unsigned w = (swiz & 0xc0) >> 6;

   gc_string_append(string, ".");
   gc_decode_swiz_comp(string, x);
   gc_decode_swiz_comp(string, y);
   gc_decode_swiz_comp(string, z);
   gc_decode_swiz_comp(string, w);
}

static void
gc_decode_condition(struct gc_string *string, const enum gc_cond condition)
{
   switch (condition)
   {
   case GC_COND_TRUE:
      /* nothing to do */
      break;

   case GC_COND_GT:
      gc_string_append(string, ".gt");
      break;

   case GC_COND_LT:
      gc_string_append(string, ".lt");
      break;

   case GC_COND_GE:
      gc_string_append(string, ".ge");
      break;

   case GC_COND_LE:
      gc_string_append(string, ".le");
      break;

   case GC_COND_EQ:
      gc_string_append(string, ".eq");
      break;

   case GC_COND_NE:
      gc_string_append(string, ".ne");
      break;

   case GC_COND_AND:
      gc_string_append(string, ".and");
      break;

   case GC_COND_OR:
      gc_string_append(string, ".or");
      break;

   case GC_COND_XOR:
      gc_string_append(string, ".xor");
      break;

   case GC_COND_NOT:
      gc_string_append(string, ".not");
      break;

   case GC_COND_NZ:
      gc_string_append(string, ".nz");
      break;

   case GC_COND_GEZ:
      gc_string_append(string, ".gez");
      break;

   case GC_COND_GZ:
      gc_string_append(string, ".gz");
      break;

   case GC_COND_LEZ:
      gc_string_append(string, ".lez");
      break;

   case GC_COND_LZ:
      gc_string_append(string, ".lz");
      break;
   }
}

static void
gc_decode_dst(struct gc_string *string, const struct gc_instr_dst *dst)
{
   gc_string_append(string, "t%u", dst->reg);
   gc_decode_address_mode(string, dst->amode);
   gc_decode_components(string, dst->comps);

   gc_string_append(string, ", ");
}

static void
gc_decode_src(struct gc_string *string, const struct gc_instr_src *src,
              const bool sep)
{
   if (src->use) {
      if (src->neg)
         gc_string_append(string, "-");

      if (src->abs)
         gc_string_append(string, "|");

      gc_decode_gc_register_group(string, src->rgroup);
      gc_string_append(string, "%u", src->reg);
      gc_decode_address_mode(string, src->amode);
      gc_decode_swiz(string, src->swiz);

      if (src->abs)
         gc_string_append(string, "|");

   } else {
      gc_string_append(string, "void");
   }

   if (sep)
      gc_string_append(string, ", ");
}

static void
gc_decode_alu(struct gc_string *string, const struct gc_instr *instr)
{
   const struct gc_instr_alu *alu = &instr->alu;

   gc_decode_src(string, &alu->src[0], true);
   gc_decode_src(string, &alu->src[1], true);
   gc_decode_src(string, &alu->src[2], false);
}

static void
gc_decode_branch(struct gc_string *string, const struct gc_instr *instr)
{
   const struct gc_instr_branch *branch = &instr->branch;

   gc_decode_src(string, &branch->src[0], true);
   gc_decode_src(string, &branch->src[1], true);
   gc_string_append(string, "label_%04u", branch->imm);
}

static void
gc_decode_sampler(struct gc_string *string, const struct gc_instr *instr)
{
   const struct gc_instr_sampler *sampler = &instr->sampler;

   gc_string_append(string, "tex%u", sampler->id);
   gc_decode_address_mode(string, sampler->amode);
   gc_decode_swiz(string, sampler->swiz);
   gc_string_append(string, ", ");
   gc_decode_src(string, &sampler->src, true);
   gc_string_append(string, "void, void");
}

static void
gc_decode(struct gc_string *string, const struct gc_instr *instr)
{
   const bool has_dst = gc_op_has_dst(instr->opcode);

   /* lets start filling buffer from offset 0 */
   string->offset = 0;

   gc_string_append(string, "%s", gc_op_name(instr->opcode));
   gc_decode_operand_type(string, instr->operand_type);
   if (instr->saturate)
      gc_string_append(string, ".sat");

   gc_decode_condition(string, instr->condition);

   /* add some space before starting with operands */
   gc_string_pad_to(string, 0, 11);

   if (has_dst)
      gc_decode_dst(string, &instr->dst);
   else
      gc_string_append(string, "void, ");

   const enum gc_op_type type = gc_op_type(instr->opcode);
   switch (type) {
   case GC_OP_TYPE_ALU:
      gc_decode_alu(string, instr);
      break;
   case GC_OP_TYPE_BRANCH:
      gc_decode_branch(string, instr);
      break;
   case GC_OP_TYPE_SAMPLER:
      gc_decode_sampler(string, instr);
      break;
   }
}

/**
 * Returns a string containing the disassembled representation of the GC
 * instruction. It is the caller's responsibility to free the return value
 * with ralloc_free().
 */
void
gc_disasm(struct gc_string *string, const uint32_t *inst)
{
   assert(string);
   assert(inst);

   struct gc_instr instr;

   gc_unpack_instr(inst, &instr);

   gc_decode(string, &instr);
}

/**
 * Returns a string containing the disassembled representation of the GC
 * instruction. It is the caller's responsibility to free the return value
 * with ralloc_free().
 */
const char *
gc_dump_string(const struct gc_instr *instr)
{
   struct gc_string string = {
      .string = (char *)rzalloc_size(NULL, 1),
      .offset = 0,
   };

   gc_decode(&string, instr);

   return string.string;
}
