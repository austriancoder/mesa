#include "tegra_drm_public.h"

#include "renderonly/renderonly.h"
#include "../winsys/nouveau/drm/nouveau_drm_public.h"

#include <fcntl.h>
#include <drm/tegra_drm.h>
#include <xf86drm.h>

static struct pipe_screen *
tegra_create(struct renderonly *ro)
{
   int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);

   if (fd == -1)
      return NULL;

   return nouveau_drm_screen_create_renderonly(fd, ro);
}

static int
tegra_tiling(int fd, uint32_t handle)
{
   struct drm_tegra_gem_set_tiling args = {
      .handle = handle,
      .mode = DRM_TEGRA_GEM_TILING_MODE_BLOCK,
      .value = 4
   };

   return drmIoctl(fd, DRM_IOCTL_TEGRA_GEM_SET_TILING, &args);
}

static const struct renderonly_ops ro_ops = {
   .create = tegra_create,
   .tiling = tegra_tiling
};

struct pipe_screen *tegra_drm_screen_create(int fd)
{
   return renderonly_screen_create(fd, &ro_ops, NULL);
}
