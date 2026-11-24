// PS Vita platform layer. The PSP port (psp/main_psp.cpp) is the reference: same
// frame loop, same MIDP key codes, same menu rows. Differences are the renderer
// (vita2d, 640x360 -> 960x544) and the storage layout (ux0:data/gow-betrayal).
#include <vita2d.h>

#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "font8x8.inc"
#include "gen/classes.h"
#include "locale_psp.h"
#include "vita_video.h"

using namespace gow;
bool initAudio();
bool audioProducedSignal();
bool audioProducedTimGM();
void shutdownAudio();
unsigned playIntro(bool (*cancel)(), void (*log)(const char*));

namespace {
constexpr const char* kBuild = "vita-001";
constexpr const char* kHomeDir = "ux0:data/gow-betrayal";
int fps = 25;
std::atomic<bool> exiting{false};

void log(const char* message) {
  if (FILE* f = std::fopen("ux0:data/gow-betrayal/gow-vita.log", "a")) {
    std::fprintf(f, "[%s] %s\n", kBuild, message);
    std::fclose(f);
  }
}

void makeDirs() {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(kHomeDir, 0777);
  sceIoMkdir("ux0:data/gow-betrayal/saves", 0777);
}

bool hasBank(const char* dir) {
  std::string p = std::string(dir) + "/RP1";
  if (FILE* f = std::fopen(p.c_str(), "rb")) {
    std::fclose(f);
    return true;
  }
  return false;
}

// The banks are user-supplied, so they live outside the VPK: either in the data
// folder next to eboot or in the writable home directory.
void pickDirs() {
  const std::string homeData = std::string(kHomeDir) + "/data";
  if (hasBank(homeData.c_str())) {
    Platform::dataDir = std::string(kHomeDir) + "/data";
  } else if (hasBank("data")) {
    Platform::dataDir = "data";
  } else {
    Platform::dataDir = std::string(kHomeDir) + "/data";  // the path the error message names
  }
  Platform::saveDir = std::string(kHomeDir) + "/saves";
}

// ---- error screen ---------------------------------------------------------
int glyphWidth(char c) {
  if (c < 32 || c > 126) c = '?';
  return kFontAdvance[c - 32];
}

void drawChar(Surface& s, int x, int y, char c, uint32_t argb, int scale) {
  if (c < 32 || c > 126) c = '?';
  const unsigned short* rows = kFontGlyph[c - 32];
  for (int r = 0; r < kFontHeight; ++r) {
    const unsigned short bits = rows[r];
    if (!bits) continue;
    for (int col = 0; col < 16; ++col)
      if (bits & (0x8000u >> col)) s.fillRectArgb(x + col * scale, y + r * scale, scale, scale, argb);
  }
}

// Wrapped, left-aligned text. '\n' forces a break; long words break at the edge.
void drawText(Surface& s, int x0, int y, int maxW, const char* text, uint32_t argb, int scale) {
  int x = x0;
  const char* word = text;
  auto flush = [&](const char* end) {  // draw [word, end)
    int w = 0;
    for (const char* p = word; p < end; ++p) w += glyphWidth(*p) * scale;
    if (x != x0 && x + w > x0 + maxW) {
      x = x0;
      y += (kFontHeight + 4) * scale;
    }
    for (const char* p = word; p < end; ++p) {
      drawChar(s, x, y, *p, argb, scale);
      x += glyphWidth(*p) * scale;
    }
  };
  for (const char* p = text;; ++p) {
    if (*p && *p != ' ' && *p != '\n') continue;
    flush(p);
    if (!*p) break;
    if (*p == '\n') {
      x = x0;
      y += (kFontHeight + 4) * scale;
    } else {
      x += glyphWidth(' ') * scale;
    }
    word = p + 1;
  }
}

void showError(const char* message) {
  log(message);
  if (vita::gVideo.ready()) {
    Surface s(kVitaWidth, kScreenH);
    s.fillRectArgb(0, 0, kVitaWidth, kScreenH, 0xFF100808u);
    drawText(s, 24, 32, kVitaWidth - 48, "GOD OF WAR: BETRAYAL", 0xFFFFC46Bu, 2);
    drawText(s, 24, 76, kVitaWidth - 48, kBuild, 0xFF808080u, 1);
    drawText(s, 24, 104, kVitaWidth - 48, message, 0xFFFFFFFFu, 1);
    drawText(s, 24, kScreenH - 32, kVitaWidth - 48, "Press START to quit.", 0xFFA0A0A0u, 1);
    vita::gVideo.present(s);
  }
  while (!exiting) {
    SceCtrlData pad{};
    sceCtrlPeekBufferPositive(0, &pad, 1);
    if (pad.buttons & SCE_CTRL_START) break;
    sceKernelDelayThread(16000);
  }
}

// ---- input ----------------------------------------------------------------
struct Controls {
  bool sent[8]{};
  unsigned suppress = 0;
  static constexpr int codes[] = {-1, -2, -3, -4, -5, -6, -7, 35};
  void poll(Game& game, bool reset = false) {
    SceCtrlData pad{};
    if (!reset) sceCtrlPeekBufferPositive(0, &pad, 1);
    else pad.lx = pad.ly = 128;
    suppress &= pad.buttons;
    const unsigned b = pad.buttons & ~suppress;
    const bool menu = Game::state != 100;
    const bool now[] = {
        (b & SCE_CTRL_UP) || pad.ly < 64 || (!menu && (b & SCE_CTRL_CROSS)),
        (b & SCE_CTRL_DOWN) || pad.ly > 192 || (!menu && (b & SCE_CTRL_CIRCLE)),
        (b & SCE_CTRL_LEFT) || pad.lx < 64,
        (b & SCE_CTRL_RIGHT) || pad.lx > 192,
        (b & SCE_CTRL_SQUARE) != 0,
        (b & (SCE_CTRL_TRIANGLE | SCE_CTRL_R1)) || (menu && (b & SCE_CTRL_CROSS)),
        (b & SCE_CTRL_START) || (menu && (b & SCE_CTRL_CIRCLE)),
        (b & (SCE_CTRL_L1 | SCE_CTRL_SELECT)) != 0,
    };
    for (int i = 0; i < 8; ++i)
      if (sent[i] != now[i]) game.platformKey(codes[i], sent[i] = now[i]);
  }
};

double nowMs() { return (double)sceKernelGetProcessTimeWide() / 1000.0; }
}  // namespace

String Port::label(int row) {
  if (Game::languageIndex == 6 || Game::languageIndex == 7) {
    const bool ru = Game::languageIndex == 6;
    if (row == 0) return String(ru ? u"Экран: 640x360" : u"Екран: 640x360");
    if (row == 1)
      return String(ru ? (vita::gVideo.smoothScaling() ? u"Масштаб: мягкий" : u"Масштаб: чёткий")
                       : (vita::gVideo.smoothScaling() ? u"Масштаб: м'який" : u"Масштаб: чіткий"));
    return String(ru ? (fps == 30 ? u"Кадры: 30" : u"Кадры: 25")
                     : (fps == 30 ? u"Кадри: 30" : u"Кадри: 25"));
  }
  if (row == 0) return String(u"Screen: 640x360");
  if (row == 1)
    return String(vita::gVideo.smoothScaling() ? u"Pixel scale: smooth" : u"Pixel scale: sharp");
  return String(fps == 30 ? u"FPS: 30" : u"FPS: 25");
}

void Port::change(int row, int) {
  if (row == 1) vita::gVideo.setSmoothScaling(!vita::gVideo.smoothScaling());
  if (row == 2) fps = fps == 30 ? 25 : 30;
}

int Port::viewWidth() { return kVitaWidth; }
int Port::displayFps() { return fps; }

Arr<int> Port::extendMenu(const Arr<int>& t) {
  // Generated Game.cpp expects the original quit entry (23) shifted by three.
  constexpr int at = 22, n = 3;
  if (t.length() <= at) return t;
  Arr<int> o(t.length() + n);
  for (int i = 0; i < t.length(); ++i) {
    int e = t[i], target = (e >> 8) & 255;
    if (target >= at) e = (e & ~0xff00) | (((target + n) & 255) << 8);
    o[i < at ? i : i + n] = e;
  }
  for (int i = 0; i < n; ++i) o[at + i] = ((13 + i) << 26) | 0x3000000;
  return o;
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  makeDirs();
  log("boot");
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
  pickDirs();

  if (!vita::gVideo.init()) {
    log("vita2d init failed");
    showError("Video init failed (vita2d).");
    sceKernelExitProcess(0);
    return 0;
  }

  try {
    const unsigned skip = playIntro([] { return exiting.load(); }, log);
    if (exiting) {
      vita::gVideo.shutdown();
      sceKernelExitProcess(0);
      return 0;
    }
    GOWMIDlet midlet;
    midlet.startApp();
    auto& game = *static_cast<Game*>(midlet.game);
    if (!game.ok() || !game.supportedBuild()) {
      showError("Missing or incompatible data/ banks.\n"
                "Copy the K800i 1.4.20 banks prepared with\n"
                "prepare_data.py to:\n"
                "ux0:data/gow-betrayal/data");
    } else {
      Game::languageStringBanks = Arr<int>::make(10240, 12288, 33792, 29696, 30720, 32768,
                                                 kRussianBank, kUkrainianBank);
      Game::languageNameStrings = Arr<int>::make(23, 24, 21, 20, 22, 19, kRussianName, kUkrainianName);
      const auto language = Engine::readRecord(kLanguageRecord);
      if (language.length() == 1 && language[0] >= 0 && language[0] < Game::languageStringBanks.length())
        Game::languageIndex = language[0];

      Surface screen(kVitaWidth, kScreenH);
      Controls input;
      input.suppress = skip;
      log(initAudio() ? "audio ready" : "audio unavailable");
      log("game initialized");

      int lastState = Game::state;
      double reportAt = nowMs() + 5000;
      long startTicks = 0, startPaints = 0;
      while (!exiting && !game.quit) {
        input.poll(game);
        const double start = nowMs();
        const auto old = game.paintCount;
        const int wait = game.runFrame(start, screen);
        if (game.paintCount != old) vita::gVideo.present(screen);

        if (lastState != Game::state || start >= reportAt) {
          char info[160];
          if (lastState != Game::state) {
            lastState = Game::state;
            std::snprintf(info, sizeof info, "state=%d", lastState);
            log(info);
          }
          if (start >= reportAt) {
            std::snprintf(info, sizeof info,
                          "5s: state=%d ticks=%ld paints=%ld player=%d,%d action=%d",
                          lastState, game.tickCount - startTicks, game.paintCount - startPaints,
                          game.playerX, game.playerY, game.playerAction);
            log(info);
            reportAt = start + 5000;
            startTicks = game.tickCount;
            startPaints = game.paintCount;
          }
        }
        // The engine returns the time until the next deadline, drawing included.
        const double deadline = start + wait;
        while (!exiting) {
          const int remaining = (int)((deadline - nowMs()) * 1000);
          if (remaining <= 0) break;
          sceKernelDelayThread(std::min(remaining, 8000));
          input.poll(game);
        }
      }
      if (!game.quit) game.postLifecycle(3);
    }
  } catch (const JavaException& e) {
    showError(e.what);
  } catch (const std::exception& e) {
    showError(e.what());
  }
  shutdownAudio();
  log("exit");
  vita::gVideo.shutdown();
  sceKernelExitProcess(0);
  return 0;
}
