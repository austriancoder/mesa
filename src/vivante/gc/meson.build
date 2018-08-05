# Copyright (c) 2018 Etnaviv Project
# Copyright (C) 2018 Zodiac Inflight Innovations
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sub license,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

libvivante_gc_files = files(
  'gc_disasm.c',
  'gc_disasm.h',
  'gc_instr.c',
  'gc_instr.h',
  'gc_pack.c',
  'isa.xml.h',
)

libvivante_gc = static_library(
  ['vivante_gc'],
  libvivante_gc_files,
  include_directories : [inc_common, inc_vivante],
  c_args : [c_vis_args, no_override_init_args],
  dependencies : [dep_libdrm, dep_valgrind],
  build_by_default : true,
)

subdir('tests')
