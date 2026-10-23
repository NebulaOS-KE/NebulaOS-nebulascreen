#ifndef __NEBULASCREEN_FB_H__
#define __NEBULASCREEN_FB_H__

#include <cstddef>
#include <cstdint>

// NebulaScreen's own Linux framebuffer output (G97, Run 18), replacing
// lv_drivers' fbdev_init()/fbdev_flush() on the printer.
//
// Why: Tim saw ~0.5 s of black between the boot card and the app's first
// frame. lv_drivers' fbdev_init() - and GuppyScreen::init() right after it -
// issue FBIOBLANK(FB_BLANK_UNBLANK). On the KE's Ingenic DPU that is not a
// no-op for a panel that is already on: ingenicfb_blank() -> dpu_ctrl_resume()
// stops the RDMA/compositor, re-runs the controller setup and restarts it
// (no "already unblanked" check). The panel goes black at that moment and
// stays black until the app has built all its pages and flushed its first
// frame (~0.5 s on the X2000).
//
// Now: open() maps the framebuffer WITHOUT any blank ioctl and without
// clearing it, so the boot card stays on the glass while the app builds its
// pages; GuppyScreen::loop() renders the first frame straight into the
// framebuffer (over the boot card) and only then calls unblank() once, for
// the rare case that something left the display blanked. Any DPU restart
// that ioctl still causes now shows the app's own frame.
//
// LVGL-free (host-tested: blit()).
//
// This file may be distributed under the terms of the GNU GPLv3 license.

namespace nsfb {

struct Fb {
  int fd = -1;
  uint8_t *mem = nullptr;
  size_t size = 0;            // mapped bytes
  uint32_t xres = 0, yres = 0;
  uint32_t bpp = 0;           // bits per pixel: 32 (24 is treated as 32, like lv_drivers) or 16
  uint32_t line_length = 0;   // bytes per framebuffer line
  uint32_t xoffset = 0, yoffset = 0;
};

// Opens and maps `path` (no FBIOBLANK, no clearing). False on any failure;
// `fb` stays unusable (blit() then does nothing).
bool open(const char *path, Fb &fb);

// FBIOBLANK(FB_BLANK_UNBLANK); no-op without an open framebuffer.
void unblank(const Fb &fb);

// Copies the inclusive area (x1,y1)-(x2,y2) of `src` into the framebuffer.
// `src` holds the whole area row by row (width x2-x1+1), with the
// framebuffer's pixel size (4 bytes for 32/24 bpp, 2 for 16). Parts outside
// the screen are clipped; other depths are ignored.
void blit(const Fb &fb, int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void *src);

// The printer's framebuffer (/dev/fb0), shared by main.cpp and GuppyScreen.
Fb &device();

}  // namespace nsfb

#endif  // __NEBULASCREEN_FB_H__
