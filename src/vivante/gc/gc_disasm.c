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
#include "util/ralloc.h"

struct disasm_state {
   char *string;
   size_t offset;
};

static void
append(struct disasm_state *disasm, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   ralloc_vasprintf_rewrite_tail(&disasm->string,
                                 &disasm->offset,
                                 fmt, args);
   va_end(args);
}

static void
gc_decode_operand_type(struct disasm_state *disasm,
                       const enum gc_operand_type type)
{
   switch (type) {
   case GC_OPERAND_F32:
      /* nothing to output */
      break;

   case GC_OPERAND_S32:
      append(disasm, ".s32");
      break;

   case GC_OPERAND_S8:
      append(disasm, ".s8");
      break;

   case GC_OPERAND_U16:
      append(disasm, ".u16");
      break;

   case GC_OPERAND_F16:
      append(disasm, ".f16");
      break;

   case GC_OPERAND_S16:
      append(disasm, ".s16");
      break;

   case GC_OPERAND_U32:
      append(disasm, ".u32");
      break;

   case GC_OPERAND_U8:
      append(disasm, ".u8");
      break;
   }
}

static void
gc_decode_address_mode(struct disasm_state *disasm,
                       const enum gc_addressing_mode mode)
{
   switch (mode) {
   case GC_ADDRESSING_MODE_DIRECT:
      /* nothing to output */
      break;

   case GC_ADDRESSING_MODE_ADD_A_X:
      append(disasm, "[a.x]");
      break;

   case GC_ADDRESSING_MODE_ADD_A_Y:
      append(disasm, "[a.y]");
      break;

   case GC_ADDRESSING_MODE_ADD_A_Z:
      append(disasm, "[a.z]");
      break;

   case GC_ADDRESSING_MODE_ADD_A_W:
      append(disasm, "[a.w]");
      break;
   }
}

static void
gc_decode_gc_register_group(struct disasm_state *disasm,
                            const enum gc_register_group rgoup)
{
   switch (rgoup) {
   case GC_REGISTER_GROUP_INTERNAL:
      append(disasm, "i");
      break;

   case GC_REGISTER_GROUP_TEMP:
      append(disasm, "t");
      break;

   case GC_REGISTER_GROUP_UNIFORM:
      append(disasm, "u");
      break;
   }
}

static void
gc_decode_components(struct disasm_state *disasm, const uint8_t components)
{
   if (components == 15)
      return;

   append(disasm, ".");
   if (components & INST_COMPS_X)
      append(disasm, "x");
   else
      append(disasm, "_");

   if (components & INST_COMPS_Y)
      append(disasm, "y");
   else
      append(disasm, "_");

   if (components & INST_COMPS_Z)
      append(disasm, "z");
   else
      append(disasm, "_");

   if (components & INST_COMPS_W)
      append(disasm, "w");
   else
      append(disasm, "_");
}

static inline void
gc_decode_swiz_comp(struct disasm_state *disasm, const uint8_t swiz_comp)
{
   switch (swiz_comp) {
   case INST_SWIZ_COMP_X:
      append(disasm, "x");
      break;

   case INST_SWIZ_COMP_Y:
      append(disasm, "y");
      break;

   case INST_SWIZ_COMP_Z:
      append(disasm, "z");
      break;

   case INST_SWIZ_COMP_W:
      append(disasm, "w");
      break;

   default:
      unreachable("unsupported swizling");
      break;
   }
}

static void
gc_decode_swiz(struct disasm_state *disasm, const uint8_t swiz)
{
   /* if a null swizzle */
   if (swiz == 0xe4)
      return;

   const unsigned x = swiz & 0x3;
   const unsigned y = (swiz & 0x0C) >> 2;
   const unsigned z = (swiz & 0x30) >> 4;
   const unsigned w = (swiz & 0xc0) >> 6;

   append(disasm, ".");
   gc_decode_swiz_comp(disasm, x);
   gc_decode_swiz_comp(disasm, y);
   gc_decode_swiz_comp(disasm, z);
   gc_decode_swiz_comp(disasm, w);
}

static void
gc_decode_condition(struct disasm_state *disasm, const enum gc_cond condition)
{
   switch (condition)
   {
   case GC_COND_TRUE:
      /* nothing to do */
      break;

   case GC_COND_GT:
      append(disasm, ".gt");
      break;

   case GC_COND_LT:
      append(disasm, ".lt");
      break;

   case GC_COND_GE:
      append(disasm, ".ge");
      break;

   case GC_COND_LE:
      append(disasm, ".le");
      break;

   case GC_COND_EQ:
      append(disasm, ".eq");
      break;

   case GC_COND_NE:
      append(disasm, ".ne");
      break;

   case GC_COND_AND:
      append(disasm, ".and");
      break;

   case GC_COND_OR:
      append(disasm, ".or");
      break;

   case GC_COND_XOR:
      append(disasm, ".xor");
      break;

   case GC_COND_NOT:
      append(disasm, ".not");
      break;

   case GC_COND_NZ:
      append(disasm, ".nz");
      break;

   case GC_COND_GEZ:
      append(disasm, ".gez");
      break;

   case GC_COND_GZ:
      append(disasm, ".gz");
      break;

   case GC_COND_LEZ:
      append(disasm, ".lez");
      break;

   case GC_COND_LZ:
      append(disasm, ".lz");
      break;
   }
}

static void
gc_decode_dst(struct disasm_state *disasm, const struct gc_instr_dst *dst)
{
   append(disasm, "t%u", dst->reg);
   gc_decode_address_mode(disasm, dst->amode);
   gc_decode_components(disasm, dst->comps);

   append(disasm, ", ");
}

static void
gc_decode_src(struct disasm_state *disasm, const struct gc_instr_src *src,
              const bool sep)
{
   if (src->use) {
      if (src->neg)
         append(disasm, "-");

      if (src->abs)
         append(disasm, "|");

      gc_decode_gc_register_group(disasm, src->rgroup);
      append(disasm, "%u", src->reg);
      gc_decode_address_mode(disasm, src->amode);
      gc_decode_swiz(disasm, src->swiz);

      if (src->abs)
         append(disasm, "|");

   } else {
      append(disasm, "void");
   }

   if (sep)
      append(disasm, ", ");
}

static void
gc_decode_alu(struct disasm_state *disasm, const struct gc_instr *instr)
{
   const struct gc_instr_alu *alu = &instr->alu;

   gc_decode_src(disasm, &alu->src[0], true);
   gc_decode_src(disasm, &alu->src[1], true);
   gc_decode_src(disasm, &alu->src[2], false);
}

static void
gc_decode_branch(struct disasm_state *disasm, const struct gc_instr *instr)
{
   const struct gc_instr_branch *branch = &instr->branch;

   gc_decode_src(disasm, &branch->src[0], true);
   gc_decode_src(disasm, &branch->src[1], true);
   append(disasm, "label_%04u", branch->imm);
}

static void
gc_decode_sampler(struct disasm_state *disasm, const struct gc_instr *instr)
{
   const struct gc_instr_sampler *sampler = &instr->sampler;

   append(disasm, "tex%u", sampler->id);
   gc_decode_address_mode(disasm, sampler->amode);
   gc_decode_swiz(disasm, sampler->swiz);
   append(disasm, ", ");
   gc_decode_src(disasm, &sampler->src, true);
   append(disasm, "void, void");
}

static const char *
gc_decode(const struct gc_instr *instr)
{
   const bool has_dst = gc_op_has_dst(instr->opcode);

   struct disasm_state disasm = {
      .string = rzalloc_size(NULL, 1),
      .offset = 0,
   };

   append(&disasm, "%s", gc_op_name(instr->opcode));
   gc_decode_operand_type(&disasm, instr->operand_type);
   if (instr->saturate)
      append(&disasm, ".sat");

   gc_decode_condition(&disasm, instr->condition);

   // add some space before starting with operands
   while (disasm.offset < 11)
      append(&disasm, " ");

   if (has_dst)
      gc_decode_dst(&disasm, &instr->dst);
   else
      append(&disasm, "void, ");

   const enum gc_op_type type = gc_op_type(instr->opcode);
   switch (type) {
   case GC_OP_TYPE_ALU:
      gc_decode_alu(&disasm, instr);
      break;
   case GC_OP_TYPE_BRANCH:
      gc_decode_branch(&disasm, instr);
      break;
   case GC_OP_TYPE_SAMPLER:
      gc_decode_sampler(&disasm, instr);
      break;
   }

   return disasm.string;
}

/**
 * Returns a string containing the disassembled representation of the GC
 * instruction. It is the caller's responsibility to free the return value
 * with ralloc_free().
 */
const char *
gc_disasm(const uint32_t *inst)
{
   struct gc_instr instr;
   gc_unpack_instr(inst, &instr);
   return gc_decode(&instr);
}

/**
 * Returns a string containing the disassembled representation of the GC
 * instruction. It is the caller's responsibility to free the return value
 * with ralloc_free().
 */
const char *
gc_dump_string(const struct gc_instr *instr)
{
   return gc_decode(instr);
}

int
gc_dump(const struct gc_instr *instr)
{
   const char *decoded = gc_decode(instr);
   int len = fprintf(stderr, "%s", decoded);
   ralloc_free((void *)decoded);
   return len;
}
