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

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "main/mtypes.h"

#include "compiler/eir_compiler.h"
#include "compiler/eir_nir.h"
#include "compiler/eir_shader.h"
#include "compiler/glsl/standalone.h"
#include "compiler/glsl/glsl_to_nir.h"

#include "etnaviv_eir.h"

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
   static struct gl_context local_ctx;
	struct gl_shader_program *prog;

	prog = standalone_compile_shader(&options, 1, (char * const*)&filename, &local_ctx);
	if (!prog)
		errx(1, "couldn't parse `%s'", filename);

	nir_shader *nir = glsl_to_nir(&local_ctx, prog, stage, eir_get_compiler_options());

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

      nir = eir_tgsi_to_nir(toks, NULL);
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

   struct eir_compiler *c = eir_compiler_create();

   if (verbose)
      eir_compiler_debug |= EIR_DBG_OPTMSGS;

   struct eir_shader *s = eir_shader_from_nir(c, nir);
   bool created = false;
   struct eir_shader_variant *v = eir_shader_get_variant(s, key, &created);

   eir_dump_shader(v);
   eir_shader_destroy(s);
   eir_compiler_free(c);

   return ret;
}
