#define _CRT_SECURE_NO_WARNINGS
#include "gfx.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace gow {

void Surface::setClip(int x, int y, int cw, int chh) {
  x += tx_;
  y += ty_;
  cx_ = std::max(0, x);
  cy_ = std::max(0, y);
  cx2_ = std::min(w, x + cw);
  cy2_ = std::min(h, y + chh);
}

void Surface::plot(int x, int y, uint32_t argb) {
  x += tx_;
  y += ty_;
  if (x < cx_ || x >= cx2_ || y < cy_ || y >= cy2_) return;
  argb = nativeColor(argb);
  uint32_t a = argb >> 24;
  uint32_t& d = px[(size_t)y * pitch() + x];
  if (a == 255) {
    d = argb;
  } else if (a) {
    uint32_t r = ((argb >> 16 & 255) * a + (d >> 16 & 255) * (255 - a)) / 255;
    uint32_t g = ((argb >> 8 & 255) * a + (d >> 8 & 255) * (255 - a)) / 255;
    uint32_t b = ((argb & 255) * a + (d & 255) * (255 - a)) / 255;
    d = 0xFF000000u | r << 16 | g << 8 | b;
  }
}

void Surface::fillRectArgb(int x, int y, int rw, int rh, uint32_t argb) {
  x += tx_;
  y += ty_;
  int x0 = std::max(x, cx_), y0 = std::max(y, cy_);
  int x1 = std::min(x + rw, cx2_), y1 = std::min(y + rh, cy2_);
  if (x0 >= x1 || y0 >= y1) return;
  const uint32_t color = nativeColor(argb);
  for (int yy = y0; yy < y1; yy++)
    std::fill_n(px.data() + (size_t)yy * pitch() + x0, x1 - x0, color);
}

void Surface::drawRect(int x, int y, int rw, int rh) {
  if (rw < 0 || rh < 0) return;
  fillRect(x, y, rw + 1, 1);
  fillRect(x, y + rh, rw + 1, 1);
  fillRect(x, y, 1, rh + 1);
  fillRect(x + rw, y, 1, rh + 1);
}

void Surface::drawLine(int x1, int y1, int x2, int y2) {
  int dx = std::abs(x2 - x1), dy = -std::abs(y2 - y1);
  int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx + dy;
  for (;;) {
    plot(x1, y1, color_);
    if (x1 == x2 && y1 == y2) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x1 += sx; }
    if (e2 <= dx) { err += dx; y1 += sy; }
  }
}

void Surface::fillArc(int x, int y, int rw, int rh, int, int) {  // full ellipses only
  if (rw <= 0 || rh <= 0) return;
  double a = rw / 2.0, b = rh / 2.0, cx = x + a, cy = y + b;
  for (int yy = y; yy < y + rh; yy++)
    for (int xx = x; xx < x + rw; xx++) {
      double u = (xx + 0.5 - cx) / a, v = (yy + 0.5 - cy) / b;
      if (u * u + v * v <= 1.0) plot(xx, yy, color_);
    }
}

void Surface::fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t argb) {
  int minX = std::min({x1, x2, x3}), maxX = std::max({x1, x2, x3});
  int minY = std::min({y1, y2, y3}), maxY = std::max({y1, y2, y3});
  auto edge = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
  int area = edge(x1, y1, x2, y2, x3, y3);
  if (!area) return;
  for (int yy = minY; yy <= maxY; yy++)
    for (int xx = minX; xx <= maxX; xx++) {
      int w0 = edge(x2, y2, x3, y3, xx, yy), w1 = edge(x3, y3, x1, y1, xx, yy), w2 = edge(x1, y1, x2, y2, xx, yy);
      if (area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0)) plot(xx, yy, argb);
    }
}

Font* Surface::getFont() {
  static Font f;
  return &f;
}

void Surface::blit(const Image& src, int sx, int sy, int rw, int rh, int trans, int dx, int dy) {
  bool mx = (trans & 2) != 0, my = (trans & 1) != 0;
  dx += tx_;
  dy += ty_;
  // Intersect the destination clip and the source bounds once, rather than per pixel.
  int i0 = std::max({0, cx_ - dx, mx ? sx + rw - src.w : -sx});
  int i1 = std::min({rw, cx2_ - dx, mx ? sx + rw : src.w - sx});
  int j0 = std::max({0, cy_ - dy, my ? sy + rh - src.h : -sy});
  int j1 = std::min({rh, cy2_ - dy, my ? sy + rh : src.h - sy});
  if (i0 >= i1 || j0 >= j1) return;
  int dir = mx ? -1 : 1;
  int from = sx + (mx ? rw - 1 - i0 : i0);
  const int srcPitch = src.pitch(), dstPitch = pitch();
  for (int j = j0; j < j1; ++j) {
    const auto* srcRow = src.px.data() + (size_t)(sy + (my ? rh - 1 - j : j)) * srcPitch;
    auto* dst = px.data() + (size_t)(dy + j) * dstPitch + (dx + i0);
    for (int i = 0, x = from; i < i1 - i0; ++i, x += dir) {
      uint32_t p = srcRow[x];
      uint32_t a = p >> 24;
      if (a == 0) continue;
      uint32_t& d = dst[i];
      if (a == 255) {
        d = p;
      } else {
        uint32_t r = ((p >> 16 & 255) * a + (d >> 16 & 255) * (255 - a)) / 255;
        uint32_t g = ((p >> 8 & 255) * a + (d >> 8 & 255) * (255 - a)) / 255;
        uint32_t b = ((p & 255) * a + (d & 255) * (255 - a)) / 255;
        d = 0xFF000000u | r << 16 | g << 8 | b;
      }
    }
  }
}

// MIDP anchors: HCENTER 1, VCENTER 2, LEFT 4, RIGHT 8, TOP 16, BOTTOM 32.
static void anchorPos(int anchor, int rw, int rh, int& x, int& y) {
  if (anchor & 1) x -= rw >> 1;
  else if (anchor & 8) x -= rw;
  if (anchor & 2) y -= rh >> 1;
  else if (anchor & 32) y -= rh;
}

void Surface::drawImage(const Image* img, int x, int y, int anchor) {
  if (!img) return;
  anchorPos(anchor, img->w, img->h, x, y);
  blit(*img, 0, 0, img->w, img->h, 0, x, y);
}

void Surface::drawRegion(const Image* img, int sx, int sy, int rw, int rh, int trans, int dx, int dy, int anchor) {
  if (!img) return;
  anchorPos(anchor, rw, rh, dx, dy);
  blit(*img, sx, sy, rw, rh, trans, dx, dy);
}

void Surface::drawRGB(const Arr<int>& rgb, int offset, int scanlength, int x, int y, int rw, int rh, bool processAlpha) {
  if (!processAlpha && rw > 0 && rh > 0 && scanlength >= rw && offset >= 0 &&
      (int64_t)offset + (int64_t)(rh - 1) * scanlength + rw <= rgb.length()) {
    const int dx = x + tx_, dy = y + ty_;
    int left = std::max(0, cx_ - dx), right = std::min(rw, cx2_ - dx);
    int top = std::max(0, cy_ - dy), bottom = std::min(rh, cy2_ - dy);
    if (left >= right || top >= bottom) return;
    for (int j = top; j < bottom; ++j) {
      const int* src = rgb.data() + offset + j * scanlength + left;
      uint32_t* dst = px.data() + (size_t)(dy + j) * pitch() + (dx + left);
      for (int i = 0; i < right - left; ++i) dst[i] = nativeColor((uint32_t)src[i] | 0xff000000u);
    }
    return;
  }
  for (int j = 0; j < rh; j++) {
    for (int i = 0; i < rw; i++) {
      uint32_t p = (uint32_t)rgb[offset + j * scanlength + i];
      if (!processAlpha) p |= 0xFF000000u;
      plot(x + i, y + j, p);
    }
  }
}

bool writeBmp(const Surface& s, const char* path) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  uint32_t size = 54 + s.w * s.h * 4;
  uint8_t hdr[54] = {'B', 'M'};
  auto put = [&](int off, uint32_t v) { for (int i = 0; i < 4; i++) hdr[off + i] = (uint8_t)(v >> (8 * i)); };
  put(2, size); put(10, 54); put(14, 40); put(18, (uint32_t)s.w); put(22, (uint32_t)-s.h);
  hdr[26] = 1; hdr[28] = 32;
  std::fwrite(hdr, 1, 54, f);
  for (int y = 0; y < s.h; ++y) {
    auto* row = s.px.data() + (size_t)y * s.pitch();
#if defined(__PSP__) || defined(__vita__)
    for (int x = 0; x < s.w; ++x) {
      uint32_t c = nativeColor(row[x]);  // red/blue swap is its own inverse
      std::fwrite(&c, 4, 1, f);
    }
#else
    std::fwrite(row, 4, s.w, f);
#endif
  }
  std::fclose(f);
  return true;
}

}  // namespace gow
