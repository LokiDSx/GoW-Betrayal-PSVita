// vita2d presentation of the game's software framebuffer.
//
// The game renders kVitaWidth x kScreenH (480x272) ABGR8888 words (see nativeColor)
// into a Surface; Video uploads them into one texture and scales it x2 to the
// panel's 960x544. The chain is entirely integer, so each game pixel is an
// exact 2x2 block on screen.
#pragma once
#include <cstdint>

#include "gfx.h"

namespace gow {
namespace vita {

class Video {
 public:
  // Creates the vita2d context and the frame texture. False = cannot draw at all.
  bool init();
  bool ready() const { return tex_ != nullptr; }

  // Uploads s (any size up to kVitaWidth x kScreenH) and pushes it to the display.
  // Areas of the texture outside s are cleared to opaque black first.
  void present(const Surface& s);

  // Same, from a raw RGB565 frame of w x h (the PINT intro): it is scaled to the
  // whole panel, so a 480x272 source lands on exact 2x pixels.
  void present565(const uint16_t* rgb565, int w, int h);

  // true = bilinear, false = point. Point keeps the game's 240->480->960 pixel
  // blocks exactly 2 screen pixels wide.
  void setSmoothScaling(bool smooth);
  bool smoothScaling() const { return smooth_; }

  void shutdown();

 private:
  uint32_t* lock();  // texture pixels, or nullptr when not ready

  void* tex_ = nullptr;   // vita2d_texture*, kVitaWidth x kScreenH
  void* tex16_ = nullptr; // second texture for non-native intro frames
  int w16_ = 0, h16_ = 0;
  unsigned rowBytes_ = 0, rowBytes16_ = 0;
  bool smooth_ = false;
};

// One process-wide instance: the game loop and the intro share it.
extern Video gVideo;

}  // namespace vita
}  // namespace gow
