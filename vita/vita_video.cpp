#include "vita_video.h"

#include <vita2d.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gow {
namespace vita {

namespace {

constexpr float kScaleX = 960.0f / kVitaWidth;
constexpr float kScaleY = 544.0f / kScreenH;

// The renderer has no logger of its own; mirror the game log's path.
void videoLog(const char* text) {
  if (FILE* f = std::fopen("ux0:data/gow-betrayal/gow-vita.log", "a")) {
    std::fprintf(f, "[vita-001] %s\n", text);
    std::fclose(f);
  }
}

// sceGxmTextureGetStride reports texels on some builds and bytes on others; both
// agree once the texture is at least its own width * 4 bytes wide.
unsigned rowBytesOf(const vita2d_texture* t) {
  const unsigned st = vita2d_texture_get_stride(t);
  return st >= vita2d_texture_get_width(t) * 4 ? st : st * 4;
}

void setPointFilter(vita2d_texture* t, bool smooth) {
  const auto f = smooth ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
  vita2d_texture_set_filters(t, f, f);
}

}  // namespace

Video gVideo;

bool Video::init() {
  if (tex_) return true;
  // This vita2d build returns 1 on success on every path (the budget-info
  // failure falls back to sceGxmInitialize), so only a negative value is fatal.
  const int started = vita2d_init();
  char note[64];
  std::snprintf(note, sizeof note, "vita2d_init returned %d", started);
  videoLog(note);
  if (started < 0) return false;
  vita2d_set_clear_color(0xFF000000);
  tex_ = vita2d_create_empty_texture(kVitaWidth, kScreenH);
  if (!tex_) {
    videoLog("create_empty_texture failed");
    vita2d_fini();
    return false;
  }
  auto* t = static_cast<vita2d_texture*>(tex_);
  setPointFilter(t, false);
  rowBytes_ = rowBytesOf(t);
  // present() rewrites every texel, but start from a defined picture.
  std::fill_n(static_cast<uint32_t*>(vita2d_texture_get_datap(t)),
              (size_t)rowBytes_ / 4 * kScreenH, 0xFF000000u);
  return true;
}

uint32_t* Video::lock() {
  if (!tex_) return nullptr;
  return static_cast<uint32_t*>(vita2d_texture_get_datap(static_cast<vita2d_texture*>(tex_)));
}

void Video::present(const Surface& s) {
  uint32_t* dst = lock();
  if (!dst) return;
  const int w = std::min(s.w, (int)kVitaWidth), h = std::min(s.h, (int)kScreenH);
  if (w != kVitaWidth || h != kScreenH)
    std::fill_n(dst, (size_t)rowBytes_ / 4 * kScreenH, 0xFF000000u);
  for (int y = 0; y < h; ++y)
    std::memcpy(dst + (size_t)y * (rowBytes_ / 4), s.px.data() + (size_t)y * s.pitch(),
                (size_t)w * 4);
  vita2d_start_drawing();
  vita2d_clear_screen();
  vita2d_draw_texture_scale(static_cast<vita2d_texture*>(tex_), 0.0f, 0.0f, kScaleX, kScaleY);
  vita2d_end_drawing();
  // Waits for the GPU, so the next present() may safely rewrite the texture.
  vita2d_swap_buffers();
}

void Video::present565(const uint16_t* src, int w, int h) {
  if (!tex_) return;
  if (!tex16_ || w16_ != w || h16_ != h) {
    if (tex16_) vita2d_free_texture(static_cast<vita2d_texture*>(tex16_));
    tex16_ = vita2d_create_empty_texture(w, h);
    if (!tex16_) return;
    w16_ = w;
    h16_ = h;
    rowBytes16_ = rowBytesOf(static_cast<vita2d_texture*>(tex16_));
    setPointFilter(static_cast<vita2d_texture*>(tex16_), false);
  }
  auto* t = static_cast<vita2d_texture*>(tex16_);
  auto* dst = static_cast<uint32_t*>(vita2d_texture_get_datap(t));
  const unsigned stride = rowBytes16_ / 4;
  for (int y = 0; y < h; ++y) {
    uint32_t* row = dst + (size_t)y * stride;
    const uint16_t* p = src + (size_t)y * w;
    for (int x = 0; x < w; ++x) {
      const uint32_t v = p[x];
      uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
      r = (r << 3) | (r >> 2);
      g = (g << 2) | (g >> 4);
      b = (b << 3) | (b >> 2);
      row[x] = 0xFF000000u | (b << 16) | (g << 8) | r;  // nativeColor's ABGR layout
    }
  }
  vita2d_start_drawing();
  vita2d_clear_screen();
  vita2d_draw_texture_scale(t, 0.0f, 0.0f, 960.0f / w, 544.0f / h);
  vita2d_end_drawing();
  vita2d_swap_buffers();
}

void Video::setSmoothScaling(bool smooth) {
  if (!tex_ || smooth == smooth_) return;
  smooth_ = smooth;
  setPointFilter(static_cast<vita2d_texture*>(tex_), smooth);
}

void Video::shutdown() {
  if (!tex_) return;
  vita2d_wait_rendering_done();
  if (tex16_) {
    vita2d_free_texture(static_cast<vita2d_texture*>(tex16_));
    tex16_ = nullptr;
  }
  vita2d_free_texture(static_cast<vita2d_texture*>(tex_));
  tex_ = nullptr;
  vita2d_fini();
}

}  // namespace vita
}  // namespace gow
