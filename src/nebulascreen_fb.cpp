// NebulaScreen framebuffer output (G97). See nebulascreen_fb.h.
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "nebulascreen_fb.h"

#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace nsfb {

bool open(const char *path, Fb &fb) {
  fb = Fb();
  int fd = ::open(path, O_RDWR);
  if (fd < 0) {
    return false;
  }
  struct fb_fix_screeninfo finfo;
  struct fb_var_screeninfo vinfo;
  std::memset(&finfo, 0, sizeof(finfo));
  std::memset(&vinfo, 0, sizeof(vinfo));
  if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1 || ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
    ::close(fd);
    return false;
  }
  void *mem = mmap(nullptr, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    ::close(fd);
    return false;
  }
  // Deliberately no FBIOBLANK and no memset: what is on the glass (the boot
  // card) stays there until the first frame replaces it.
  fb.fd = fd;
  fb.mem = static_cast<uint8_t *>(mem);
  fb.size = finfo.smem_len;
  fb.xres = vinfo.xres;
  fb.yres = vinfo.yres;
  fb.bpp = vinfo.bits_per_pixel;
  fb.line_length = finfo.line_length;
  fb.xoffset = vinfo.xoffset;
  fb.yoffset = vinfo.yoffset;
  return true;
}

void unblank(const Fb &fb) {
  if (fb.fd >= 0) {
    ioctl(fb.fd, FBIOBLANK, FB_BLANK_UNBLANK);
  }
}

void blit(const Fb &fb, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void *src) {
  if (fb.mem == nullptr || src == nullptr || x2 < x1 || y2 < y1) {
    return;
  }
  size_t px;
  if (fb.bpp == 32 || fb.bpp == 24) {
    px = 4;
  } else if (fb.bpp == 16) {
    px = 2;
  } else {
    return;
  }
  const int32_t src_w = x2 - x1 + 1;
  const int32_t cx1 = x1 < 0 ? 0 : x1;
  const int32_t cy1 = y1 < 0 ? 0 : y1;
  const int32_t cx2 = x2 > (int32_t)fb.xres - 1 ? (int32_t)fb.xres - 1 : x2;
  const int32_t cy2 = y2 > (int32_t)fb.yres - 1 ? (int32_t)fb.yres - 1 : y2;
  if (cx2 < cx1 || cy2 < cy1) {
    return;
  }
  const size_t row_bytes = (size_t)(cx2 - cx1 + 1) * px;
  const uint8_t *s = static_cast<const uint8_t *>(src);
  for (int32_t y = cy1; y <= cy2; y++) {
    size_t off = (size_t)(y + fb.yoffset) * fb.line_length + (size_t)(cx1 + fb.xoffset) * px;
    if (off + row_bytes > fb.size) {
      return;
    }
    const uint8_t *row = s + ((size_t)(y - y1) * src_w + (size_t)(cx1 - x1)) * px;
    std::memcpy(fb.mem + off, row, row_bytes);
  }
}

Fb &device() {
  static Fb fb;
  return fb;
}

}  // namespace nsfb
