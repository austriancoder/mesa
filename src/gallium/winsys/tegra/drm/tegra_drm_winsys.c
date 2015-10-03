/*
 * Copyright © 2014 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "renderonly/renderonly_screen.h"
#include "../winsys/tegra/drm/tegra_drm_public.h"
#include "../winsys/nouveau/drm/nouveau_drm_public.h"

#include <drm/tegra_drm.h>
#include <xf86drm.h>

static int tegra_tiling(int fd, uint32_t handle)
{
	struct drm_tegra_gem_set_tiling args;

	memset(&args, 0, sizeof(args));
	args.handle = handle;
	args.mode = DRM_TEGRA_GEM_TILING_MODE_BLOCK;
	args.value = 4;

	return drmIoctl(fd, DRM_IOCTL_TEGRA_GEM_SET_TILING, &args);
}

static const struct renderonly_ops ro_ops = {
	.open = nouveau_drm_screen_create,
	.tiling = tegra_tiling,
};

struct pipe_screen *tegra_drm_screen_create(int fd)
{
	return renderonly_screen_create(fd, &ro_ops);
}
