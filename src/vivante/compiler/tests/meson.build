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

test(
  'eir_assemble',
  executable(
    'eir_assemble', 'eir_assemble.cpp',
    cpp_args : [cpp_vis_args, cpp_msvc_compat_args],
    link_with: [libvivante_gc, libvivante_compiler, libmesa_util],
    include_directories: [inc_common, inc_vivante],
    dependencies : [dep_clock, dep_thread, idep_gtest],
  )
)

test(
  'eir_uniform',
  executable(
    'eir_uniform', 'eir_uniform.cpp',
    cpp_args : [cpp_vis_args, cpp_msvc_compat_args],
    link_with: [libvivante_gc, libvivante_compiler, libmesa_util],
    include_directories: [inc_common, inc_vivante],
    dependencies : [dep_clock, dep_thread, idep_gtest],
  )
)

test(
  'eir_legalize',
  executable(
    'eir_legalize', 'eir_legalize.cpp',
    cpp_args : [cpp_vis_args, cpp_msvc_compat_args],
    link_with: [libvivante_gc, libvivante_compiler, libmesa_util],
    include_directories: [inc_common, inc_vivante],
    dependencies : [dep_clock, dep_thread, idep_gtest],
  )
)
