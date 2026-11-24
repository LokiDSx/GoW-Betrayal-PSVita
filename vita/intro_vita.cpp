// The optional PINT intro video (see psp/intro_psp.cpp for the format). Frames are
// decoded to RGB565 at their native size and handed to Video, which scales them to
// the whole 960x544 panel; a 480x272 source lands on exact 2x pixels.
#include <psp2/ctrl.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

#include "vita_video.h"

namespace {
using namespace gow;
constexpr int rate = 44100, block = 512;
constexpr unsigned skipButtons = SCE_CTRL_START | SCE_CTRL_CROSS;

uint32_t word(const uint8_t* p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool inflateExact(const uint8_t* src, size_t bytes, void* dest, size_t expected) {
  uLongf actual = expected;
  return uncompress(static_cast<Bytef*>(dest), &actual, src, bytes) == Z_OK && actual == expected;
}

struct IntroAudio {
  int port = -1, thread = -1;
  const uint8_t* samples = nullptr;
  size_t bytes = 0;
  std::atomic<bool> stop{false};
  unsigned submitted = 0;

  static int run(SceSize, void* arg) {
    auto& self = **static_cast<IntroAudio**>(arg);
    alignas(64) int16_t pcm[2][2 * block];
    int slot = 0;
    for (size_t pos = 0; pos < self.bytes && !self.stop; pos += sizeof pcm[0]) {
      size_t len = std::min(self.bytes - pos, sizeof pcm[0]);
      std::memcpy(pcm[slot], self.samples + pos, len);
      std::memset(reinterpret_cast<uint8_t*>(pcm[slot]) + len, 0, sizeof pcm[0] - len);
      if (sceAudioOutOutput(self.port, pcm[slot]) < 0) break;
      ++self.submitted;
      slot ^= 1;
    }
    return 0;
  }

  void start(const std::vector<uint8_t>& sound) {
    if (sound.empty()) return;
    samples = sound.data();
    bytes = sound.size();
    port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, block, rate, SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) return;
    thread = sceKernelCreateThread("GOW intro audio", run, 0x10000080, 0x10000, 0, 0, nullptr);
    if (thread < 0) return;
    IntroAudio* self = this;
    if (sceKernelStartThread(thread, sizeof self, &self) < 0) {
      sceKernelDeleteThread(thread);
      thread = -1;
    }
  }

  void finish() {
    stop = true;
    if (thread >= 0) {
      sceKernelWaitThreadEnd(thread, nullptr, nullptr);
      sceKernelDeleteThread(thread);
      thread = -1;
    }
    if (port >= 0) { sceAudioOutReleasePort(port); port = -1; }
  }
};
}  // namespace

// PINT v1: 40-byte header, one absolute end offset per RGB565 zlib frame,
// compressed audio length, frame streams, then a zlib stream of stereo PCM.
unsigned playIntro(bool (*cancel)(), void (*log)(const char*)) {
  // The app folder is read-only on a stock console, so the home copy wins.
  FILE* f = std::fopen("ux0:data/gow-betrayal/INTRO.BIN", "rb");
  if (!f) f = std::fopen("INTRO.BIN", "rb");
  if (!f) return 0;
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::rewind(f);
  const bool sizeOk = size >= 44 && size <= 4 * 1024 * 1024;
  std::vector<uint8_t> file(sizeOk ? (size_t)size : 0);
  const bool read = sizeOk && std::fread(file.data(), 1, file.size(), f) == file.size();
  std::fclose(f);
  const auto* p = file.data();
  if (!read || std::memcmp(p, "PINT", 4) || word(p + 4) != 1) {
    log("intro ignored: invalid PINT header");
    return 0;
  }
  const int w = (int)word(p + 8), h = (int)word(p + 12);
  if (w < 64 || h < 64 || w > 1920 || h > 1080 || w * h > 1920 * 1080) {
    log("intro ignored: unsupported PINT frame size");
    return 0;
  }
  const uint32_t count = word(p + 16), fps = word(p + 20), audioBytes = word(p + 32), start = word(p + 36);
  if (!count || count > 600 || !fps || fps > 60 || count > 10 * fps ||
      word(p + 24) != rate || word(p + 28) != 2 || audioBytes % 4 || audioBytes > rate * 4 * 10 ||
      start != 44 + 4 * count || start > file.size()) {
    log("intro ignored: unsupported PINT layout");
    return 0;
  }
  std::vector<uint32_t> offsets(count + 1);
  offsets[0] = start;
  for (uint32_t i = 1; i <= count; ++i) {
    offsets[i] = word(p + 36 + 4 * i);
    if (offsets[i] <= offsets[i - 1] || offsets[i] > file.size()) {
      log("intro ignored: invalid frame table");
      return 0;
    }
  }
  const uint32_t audioPacked = word(p + 40 + 4 * count);
  if (audioPacked != file.size() - offsets.back() || (!!audioPacked != !!audioBytes)) {
    log("intro ignored: invalid audio block");
    return 0;
  }
  std::vector<uint8_t> sound(audioBytes);
  if (audioBytes && !inflateExact(p + offsets.back(), audioPacked, sound.data(), sound.size())) {
    log("intro audio damaged; playing video only");
    sound.clear();
  }
  std::vector<uint16_t> frame((size_t)w * h);
  IntroAudio audio;
  unsigned shown = 0, skip = 0;
  bool badFrame = false;
  uint64_t t0 = 0;
  char msg[96];
  std::snprintf(msg, sizeof msg, "intro playing: %dx%d", w, h);
  log(msg);
  for (uint32_t i = 0; i < count;) {
    const auto now = sceKernelGetProcessTimeWide();
    if (cancel()) break;
    SceCtrlData pad{};
    sceCtrlPeekBufferPositive(0, &pad, 1);
    if ((skip = pad.buttons & skipButtons)) break;
    if (t0) {
      const uint32_t target = (uint32_t)((now - t0) * fps / 1000000u);
      if (target < i) { sceKernelDelayThread(2000); continue; }
      i = std::min(target, count - 1);
    }
    if (!inflateExact(p + offsets[i], offsets[i + 1] - offsets[i], frame.data(),
                      (size_t)w * h * sizeof(uint16_t))) {
      badFrame = true;
      break;
    }
    vita::gVideo.present565(frame.data(), w, h);
    if (!t0) {
      t0 = sceKernelGetProcessTimeWide();
      audio.start(sound);
    }
    ++shown;
    ++i;
  }
  if (shown && !skip && !badFrame && !cancel()) {
    auto end = t0 + uint64_t(count) * 1000000 / fps;
    while (!cancel() && sceKernelGetProcessTimeWide() < end) sceKernelDelayThread(2000);
  }
  audio.finish();
  std::snprintf(msg, sizeof msg, "intro %s: %u/%u frames, %u PCM blocks",
                skip ? "skipped" : badFrame ? "decode error" : cancel() ? "interrupted" : "complete",
                shown, (unsigned)count, audio.submitted);
  log(msg);
  return skip;
}
