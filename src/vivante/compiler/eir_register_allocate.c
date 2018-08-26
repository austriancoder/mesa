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
 *    Philipp Zabel <p.zabel@pengutronix.de>
 *    Michael Tretter <m.tretter@pengutronix.de>
 */

#include "eir.h"
#include "eir_compiler.h"
#include "util/ralloc.h"
#include "util/register_allocate.h"

static const int MAX_TEMP = 64;

/* Swizzles and write masks can be used to layer virtual non-interfering
 * registers on top of the real VEC4 registers. For example, the virtual
 * VEC3_XYZ register and the virtual SCALAR_W register that use the same
 * physical VEC4 base register do not interfere.
 */
enum {
   EIR_REG_CLASS_VEC4,
   EIR_REG_CLASS_VIRT_VEC3,
   EIR_REG_CLASS_VIRT_VEC2,
   EIR_REG_CLASS_VIRT_SCALAR,
   EIR_NUM_REG_CLASSES,
} eir_reg_class;

enum {
   EIR_REG_TYPE_VEC4,
   EIR_REG_TYPE_VIRT_VEC3_XYZ,
   EIR_REG_TYPE_VIRT_VEC3_XYW,
   EIR_REG_TYPE_VIRT_VEC3_XZW,
   EIR_REG_TYPE_VIRT_VEC3_YZW,
   EIR_REG_TYPE_VIRT_VEC2_XY,
   EIR_REG_TYPE_VIRT_VEC2_XZ,
   EIR_REG_TYPE_VIRT_VEC2_XW,
   EIR_REG_TYPE_VIRT_VEC2_YZ,
   EIR_REG_TYPE_VIRT_VEC2_YW,
   EIR_REG_TYPE_VIRT_VEC2_ZW,
   EIR_REG_TYPE_VIRT_SCALAR_X,
   EIR_REG_TYPE_VIRT_SCALAR_Y,
   EIR_REG_TYPE_VIRT_SCALAR_Z,
   EIR_REG_TYPE_VIRT_SCALAR_W,
   EIR_NUM_REG_TYPES,
} eir_reg_type;

static const uint8_t
eir_reg_writemask[EIR_NUM_REG_TYPES] = {
   [EIR_REG_TYPE_VEC4] = 0xf,
   [EIR_REG_TYPE_VIRT_SCALAR_X] = 0x1,
   [EIR_REG_TYPE_VIRT_SCALAR_Y] = 0x2,
   [EIR_REG_TYPE_VIRT_VEC2_XY] = 0x3,
   [EIR_REG_TYPE_VIRT_SCALAR_Z] = 0x4,
   [EIR_REG_TYPE_VIRT_VEC2_XZ] = 0x5,
   [EIR_REG_TYPE_VIRT_VEC2_YZ] = 0x6,
   [EIR_REG_TYPE_VIRT_VEC3_XYZ] = 0x7,
   [EIR_REG_TYPE_VIRT_SCALAR_W] = 0x8,
   [EIR_REG_TYPE_VIRT_VEC2_XW] = 0x9,
   [EIR_REG_TYPE_VIRT_VEC2_YW] = 0xa,
   [EIR_REG_TYPE_VIRT_VEC3_XYW] = 0xb,
   [EIR_REG_TYPE_VIRT_VEC2_ZW] = 0xc,
   [EIR_REG_TYPE_VIRT_VEC3_XZW] = 0xd,
   [EIR_REG_TYPE_VIRT_VEC3_YZW] = 0xe,
};

static inline int eir_reg_get_type(int virt_reg)
{
   return virt_reg % EIR_NUM_REG_TYPES;
}

static inline int etna_reg_get_base(int virt_reg)
{
   return virt_reg / EIR_NUM_REG_TYPES;
}

static inline int eir_reg_get_class(int virt_reg)
{
   switch (eir_reg_get_type(virt_reg)) {
   case EIR_REG_TYPE_VEC4:
      return EIR_REG_CLASS_VEC4;
   case EIR_REG_TYPE_VIRT_VEC3_XYZ:
   case EIR_REG_TYPE_VIRT_VEC3_XYW:
   case EIR_REG_TYPE_VIRT_VEC3_XZW:
   case EIR_REG_TYPE_VIRT_VEC3_YZW:
      return EIR_REG_CLASS_VIRT_VEC3;
   case EIR_REG_TYPE_VIRT_VEC2_XY:
   case EIR_REG_TYPE_VIRT_VEC2_XZ:
   case EIR_REG_TYPE_VIRT_VEC2_XW:
   case EIR_REG_TYPE_VIRT_VEC2_YZ:
   case EIR_REG_TYPE_VIRT_VEC2_YW:
   case EIR_REG_TYPE_VIRT_VEC2_ZW:
      return EIR_REG_CLASS_VIRT_VEC2;
   case EIR_REG_TYPE_VIRT_SCALAR_X:
   case EIR_REG_TYPE_VIRT_SCALAR_Y:
   case EIR_REG_TYPE_VIRT_SCALAR_Z:
   case EIR_REG_TYPE_VIRT_SCALAR_W:
      return EIR_REG_CLASS_VIRT_SCALAR;
   default:
      unreachable("unkown register type");
   }
}

/* register-set, created one time, used for all shaders */
struct eir_ra_reg_set {
   struct ra_regs *regs;
   int class[EIR_NUM_REG_CLASSES];
};

/* Q values for the full set. Each virtual register interferes
 * with exactly one base register. And possibly with other virtual
 * registers on top of the same base register.
 */
static const unsigned int
q_val[EIR_NUM_REG_CLASSES][EIR_NUM_REG_CLASSES] = {
   { 0, 4, 6, 4 },
   { 1, 3, 6, 3 },
   { 1, 4, 4, 2 },
   { 1, 3, 3, 0 },
};

struct eir_ra_reg_set *
eir_ra_alloc_reg_set(void *memctx)
{
   struct eir_ra_reg_set *set = rzalloc(memctx, struct eir_ra_reg_set);

   set->regs = ra_alloc_reg_set(set, MAX_TEMP * EIR_NUM_REG_TYPES, true);

   for (int c = 0; c < EIR_NUM_REG_CLASSES; c++)
      set->class[c] = ra_alloc_reg_class(set->regs);

   for (int r = 0; r < EIR_NUM_REG_TYPES * MAX_TEMP; r++)
      ra_class_add_reg(set->regs, set->class[eir_reg_get_class(r)], r);

   for (int r = 0; r < MAX_TEMP; r++) {
      for (int i = 0; i < EIR_NUM_REG_TYPES; i++) {
         for (int j = 0; j < i; j++) {
            if (eir_reg_writemask[i] & eir_reg_writemask[j]) {
               ra_add_reg_conflict(set->regs, EIR_NUM_REG_TYPES * r + i,
                                         EIR_NUM_REG_TYPES * r + j);
            }
         }
      }
   }

	unsigned int **q_values = ralloc_array(set, unsigned *, EIR_NUM_REG_CLASSES);
   for (int i = 0; i < EIR_NUM_REG_CLASSES; i++) {
      q_values[i] = rzalloc_array(q_values, unsigned, EIR_NUM_REG_CLASSES);
      for (int j = 0; j < EIR_NUM_REG_CLASSES; j++)
         q_values[i][j] = q_val[i][j];
   }

   ra_set_finalize(set->regs, q_values);

   ralloc_free(q_values);

   return set;
}

static bool
interferes(const struct eir *ir, int a, int b)
{
   return !((eir_temp_range_end(ir, a) <= eir_temp_range_start(ir, b)) ||
            (eir_temp_range_end(ir, b) <= eir_temp_range_start(ir, a)));
}

static inline void
reswizzle(struct eir_register *reg, int virt_reg)
{
   static const unsigned swizzle[EIR_NUM_REG_TYPES] = {
      INST_SWIZ(0, 1, 2, 3), /* XYZW */
      INST_SWIZ(0, 1, 2, 2), /* XYZ */
      INST_SWIZ(0, 1, 3, 3), /* XYW */
      INST_SWIZ(0, 2, 3, 3), /* XZW */
      INST_SWIZ(1, 2, 3, 3), /* YZW */
      INST_SWIZ(0, 1, 1, 1), /* XY */
      INST_SWIZ(0, 2, 2, 2), /* XZ */
      INST_SWIZ(0, 3, 3, 3), /* XW */
      INST_SWIZ(1, 2, 2, 2), /* YZ */
      INST_SWIZ(1, 3, 3, 3), /* YW */
      INST_SWIZ(2, 3, 3, 3), /* ZW */
      INST_SWIZ(0, 0, 0, 0), /* X */
      INST_SWIZ(1, 1, 1 ,1), /* Y */
      INST_SWIZ(2, 2, 2, 2), /* Z */
      INST_SWIZ(3, 3, 3, 3), /* W */
   };

   const int type = eir_reg_get_type(virt_reg);
   reg->swizzle = swizzle[type];
}

static void inline
assign(struct eir_register *reg, struct ra_graph *g, unsigned *num_temps)
{
   if (reg->type != EIR_REG_TEMP)
      return;

   const int virt_reg = ra_get_node_reg(g, reg->index);

   // convert virtual register back to hw register
   // and change swizzle if needed
   reg->index = etna_reg_get_base(virt_reg);

   if (eir_reg_get_type(virt_reg) != EIR_REG_CLASS_VEC4)
      reswizzle(reg, virt_reg);

   *num_temps = MAX2(*num_temps, reg->index + 1);
}

bool
eir_register_allocate(struct eir *ir, struct eir_compiler *compiler)
{
   const int num = util_dynarray_num_elements(&ir->reg_alloc, unsigned);
   struct ra_graph *g = ra_alloc_interference_graph(compiler->set->regs, num);
   unsigned num_temps = 0;

   assert(g);

   unsigned i = 0;
   util_dynarray_foreach(&ir->reg_alloc, unsigned, num_components) {
      switch (*num_components) {
      case 1:
         ra_set_node_class(g, i, compiler->set->class[EIR_REG_CLASS_VIRT_SCALAR]);
         break;
      case 2:
         ra_set_node_class(g, i, compiler->set->class[EIR_REG_CLASS_VIRT_VEC2]);
         break;
      case 3:
         ra_set_node_class(g, i, compiler->set->class[EIR_REG_CLASS_VIRT_VEC3]);
         break;
      case 4:
         ra_set_node_class(g, i, compiler->set->class[EIR_REG_CLASS_VEC4]);
         break;
      default:
         unreachable("not supported num_components");
         break;
      }

      i++;
   }

   /* TODO: handle scalar opcodes */
#if 0
   eir_for_each_block(block, ir) {
      eir_for_each_inst(inst, block) {
         if (gc_op_is_scalar(inst->gc.opcode)) {
            ra_set_node_reg(g, )
         }
      }
   }
#endif

   for (unsigned i = 0; i < num; i++)
      for (unsigned j = 0; j < i; j++)
	      if (interferes(ir, i, j))
	         ra_add_node_interference(g, i, j);

   bool success = ra_allocate(g);
   if (!success)
      goto out;

   eir_for_each_block(block, ir) {
      eir_for_each_inst(inst, block) {
         assign(&inst->dst, g, &num_temps);
         assign(&inst->src[0], g, &num_temps);
         assign(&inst->src[1], g, &num_temps);
         assign(&inst->src[2], g, &num_temps);
      }
   }

   ir->num_temps = num_temps;

out:
   ralloc_free(g);

   return success;
}
