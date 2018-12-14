/*
 * Copyright (c) 2017 Etnaviv Project
 * Copyright (C) 2017 Zodiac Inflight Innovations
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
 *    Rob Clark <robclark@freedesktop.org>
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "compiler/eir_compiler.h"
#include "compiler/eir_nir.h"
#include "compiler/eir_shader.h"

#include "compiler/glsl/standalone.h"
#include "compiler/glsl/glsl_to_nir.h"

#include "etnaviv_eir.h"

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_text.h"

#include "util/u_debug.h"

int st_glsl_type_size(const struct glsl_type *type);

static nir_shader *
load_glsl(const char *filename, gl_shader_stage stage)
{
   static const struct standalone_options options = {
      .glsl_version = 120,
      .do_link = true,
   };
	struct gl_shader_program *prog;

	prog = standalone_compile_shader(&options, 1, (char * const*)&filename);
	if (!prog)
		errx(1, "couldn't parse `%s'", filename);

	nir_shader *nir = glsl_to_nir(prog, stage, eir_get_compiler_options());

	standalone_compiler_cleanup(prog);

	return nir;
}

static int
read_file(const char *filename, void **ptr, size_t *size)
{
   int fd, ret;
   struct stat st;

   *ptr = MAP_FAILED;

   fd = open(filename, O_RDONLY);
   if (fd == -1) {
      warnx("couldn't open `%s'", filename);
      return 1;
   }

   ret = fstat(fd, &st);
   if (ret)
      errx(1, "couldn't stat `%s'", filename);

   *size = st.st_size;
   *ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
   if (*ptr == MAP_FAILED)
      errx(1, "couldn't map `%s'", filename);

   close(fd);

   return 0;
}

static void
print_usage(void)
{
   printf("Usage: etnaviv_compiler [OPTIONS]... <file.tgsi | file.vert | file.frag>\n");
   printf("    --verbose         - verbose compiler/debug messages\n");
   printf("    --frag-rb-swap    - swap rb in color output (FRAG)\n");
   printf("    --help            - show this message\n");
}

int main(int argc, char **argv)
{
   int ret, n = 1;
   const char *filename;
   //struct eir_compiler_options options;
   struct eir_shader s;
   struct eir_shader_variant v = {};
   struct eir_shader_key key = {};
   void *ptr;
   size_t size;
   bool verbose = false;

   while (n < argc) {
      if (!strcmp(argv[n], "--verbose")) {
         verbose = true;
         n++;
         continue;
      }

      if (!strcmp(argv[n], "--frag-rb-swap")) {
         debug_printf(" %s", argv[n]);
         key.frag_rb_swap = true;
         n++;
         continue;
      }

      if (!strcmp(argv[n], "--help")) {
         print_usage();
         return 0;
      }

      break;
   }

   filename = argv[n];

   ret = read_file(filename, &ptr, &size);
   if (ret) {
      print_usage();
      return ret;
   }

   debug_printf("%s\n", (char *)ptr);
   nir_shader *nir;
   const char *ext = rindex(filename, '.');

   if (strcmp(ext, ".tgsi") == 0) {
      struct tgsi_token toks[65536];

      if (!tgsi_text_translate(ptr, toks, ARRAY_SIZE(toks)))
         errx(1, "could not parse `%s'", filename);

      nir = eir_tgsi_to_nir(toks);
   } else if (strcmp(ext, ".frag") == 0) {
      nir = load_glsl(filename, MESA_SHADER_FRAGMENT);
   } else if (strcmp(ext, ".vert") == 0) {
      nir = load_glsl(filename, MESA_SHADER_VERTEX);
   } else {
      print_usage();
      return -1;
   }

   if (should_print_nir()) {
      printf("starting point\n");
      nir_print_shader(nir, stdout);
   }

   NIR_PASS_V(nir, nir_lower_io, nir_var_all, eir_type_size_vec4, (nir_lower_io_options)0);

   s.mem_ctx = ralloc_context(NULL);
   s.compiler = eir_compiler_init();
   s.nir = eir_optimize_nir(nir);
   s.type = nir->info.stage;

   v.key = key;
   v.shader = &s;

   if (verbose)
      s.compiler->debug |= (EIR_DBG_OPTMSGS | EIR_DBG_MSGS);

   ret = eir_compile_shader_nir(s.compiler, &v);
   if (ret) {
      fprintf(stderr, "compiler failed!\n");
      return ret;
   }

   v.code = eir_assemble(v.ir, &v.info);

   eir_dump_shader(&v);

   // we can not use eir_shader_destroy(..) here so do all the
   // memory freeing this way.
   ralloc_free(s.nir);
   ralloc_free(s.mem_ctx);
   eir_compiler_free(s.compiler);
   eir_destroy(v.ir);
   free(v.code);

   return ret;
}
