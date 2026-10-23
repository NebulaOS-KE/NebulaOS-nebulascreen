// Offline tests for NebulaScreen's framebuffer output (src/nebulascreen_fb.cpp,
// G97): the blit into a framebuffer in memory, and open() failing cleanly.
//   make test-nebulascreen-fb
//
// This file may be distributed under the terms of the GNU GPLv3 license.
#include "minitest.h"
#include "../src/nebulascreen_fb.h"

#include <cstring>

#include <vector>

static nsfb::Fb mem_fb(std::vector<uint8_t> &buf, uint32_t xres, uint32_t yres, uint32_t bpp,
                       uint32_t yoffset = 0, uint32_t pad = 0) {
  nsfb::Fb fb;
  fb.xres = xres;
  fb.yres = yres;
  fb.bpp = bpp;
  uint32_t px = bpp == 16 ? 2 : 4;
  fb.line_length = xres * px + pad;
  fb.yoffset = yoffset;
  buf.assign((size_t)fb.line_length * yres * (yoffset ? 2 : 1), 0);
  fb.mem = buf.data();
  fb.size = buf.size();
  return fb;
}

static uint32_t px32(const std::vector<uint8_t> &b, const nsfb::Fb &fb, int x, int y) {
  uint32_t v;
  std::memcpy(&v, &b[(size_t)(y + fb.yoffset) * fb.line_length + (size_t)x * 4], 4);
  return v;
}

TEST(Blit, full_area_32bpp_with_line_padding) {
  std::vector<uint8_t> buf;
  auto fb = mem_fb(buf, 4, 3, 32, 0, 8);
  std::vector<uint32_t> src(12);
  for (int i = 0; i < 12; i++) src[i] = 100 + i;
  nsfb::blit(fb, 0, 0, 3, 2, src.data());
  ASSERT_EQ(px32(buf, fb, 0, 0), 100u);
  ASSERT_EQ(px32(buf, fb, 3, 0), 103u);
  ASSERT_EQ(px32(buf, fb, 0, 2), 108u);
  ASSERT_EQ(px32(buf, fb, 3, 2), 111u);
}

TEST(Blit, partial_area_and_pan_offset) {
  std::vector<uint8_t> buf;
  auto fb = mem_fb(buf, 4, 3, 32, 3);       // second buffer of a double-buffered fb
  std::vector<uint32_t> src = {7, 8, 9, 10};
  nsfb::blit(fb, 1, 1, 2, 2, src.data());
  ASSERT_EQ(px32(buf, fb, 1, 1), 7u);
  ASSERT_EQ(px32(buf, fb, 2, 1), 8u);
  ASSERT_EQ(px32(buf, fb, 1, 2), 9u);
  ASSERT_EQ(px32(buf, fb, 2, 2), 10u);
  ASSERT_EQ(px32(buf, fb, 0, 0), 0u);
  // the first buffer is untouched
  for (size_t i = 0; i < (size_t)fb.line_length * 3; i++) ASSERT_EQ((int)buf[i], 0);
}

TEST(Blit, clips_outside_the_screen_with_the_right_source_pixels) {
  std::vector<uint8_t> buf;
  auto fb = mem_fb(buf, 2, 2, 32);
  // area (-1,-1)-(2,2): 4x4 source, only the middle 2x2 lands on screen
  std::vector<uint32_t> src(16);
  for (int i = 0; i < 16; i++) src[i] = i;
  nsfb::blit(fb, -1, -1, 2, 2, src.data());
  ASSERT_EQ(px32(buf, fb, 0, 0), 5u);
  ASSERT_EQ(px32(buf, fb, 1, 0), 6u);
  ASSERT_EQ(px32(buf, fb, 0, 1), 9u);
  ASSERT_EQ(px32(buf, fb, 1, 1), 10u);
  // completely outside: nothing
  std::vector<uint8_t> before = buf;
  nsfb::blit(fb, 5, 5, 6, 6, src.data());
  ASSERT_TRUE(before == buf);
}

TEST(Blit, sixteen_bpp) {
  std::vector<uint8_t> buf;
  auto fb = mem_fb(buf, 3, 1, 16);
  std::vector<uint16_t> src = {0x1111, 0x2222};
  nsfb::blit(fb, 1, 0, 2, 0, src.data());
  uint16_t v;
  std::memcpy(&v, &buf[2], 2);
  ASSERT_EQ((int)v, 0x1111);
  std::memcpy(&v, &buf[4], 2);
  ASSERT_EQ((int)v, 0x2222);
}

TEST(Blit, nothing_without_a_framebuffer_or_with_odd_depths) {
  nsfb::Fb none;
  uint32_t px = 1;
  nsfb::blit(none, 0, 0, 0, 0, &px);   // must not crash
  std::vector<uint8_t> buf;
  auto fb = mem_fb(buf, 2, 2, 8);
  fb.bpp = 8;
  nsfb::blit(fb, 0, 0, 0, 0, &px);
  for (auto b : buf) ASSERT_EQ((int)b, 0);
}

TEST(Open, a_file_that_is_no_framebuffer_fails_cleanly) {
  nsfb::Fb fb;
  ASSERT_FALSE(nsfb::open("/dev/null", fb));
  ASSERT_TRUE(fb.mem == nullptr);
  ASSERT_EQ(fb.fd, -1);
  ASSERT_FALSE(nsfb::open("/nonexistent/fb0", fb));
  nsfb::unblank(fb);   // no-op
}

int main() {
  return minitest::run_all("nebulascreen_fb") > 0 ? 1 : 0;
}
