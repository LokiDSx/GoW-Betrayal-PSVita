// Music and SFX for the Vita: the PSP port's software MIDI synth, output through
// an SceAudio BGM port instead of a PSP audio channel. BGM (unlike MAIN) accepts
// the synth's native 44100 Hz, so no resampler is needed.
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "midi.h"
#include "platform.h"
#include "timgm.h"

namespace {
using namespace gow;
constexpr int rate = 44100, block = 512, voices = 24;
constexpr double tau = 6.283185307179586;
constexpr int waveSize = 256, waveCount = 8;
// Higher than the default user priority so the mixer wakes ahead of the renderer.
constexpr int kAudioPriority = 0x10000080;
constexpr SceSize kAudioStack = 0x10000;

int16_t wave[waveCount][waveSize];
uint32_t step[128];
alignas(64) int16_t pcm[2][block];

struct Channel {
  int program = 0, volume = 100, expression = 127, bend = 8192;
  bool pedal = false;
};
struct Voice {
  int ch = 0, key = 0, velocity = 0, shape = 0;
  int envelope = 0, age = 0;
  bool off = false, held = false;
  uint32_t phase = 0, speed = 0;
};
Channel channels[16];
Voice notes[voices];
uint32_t noise = 1;
std::shared_ptr<MidiSequence> active;
TimGM pack;
ImaVoice rendered;
const BakedTrack* activeBaked = nullptr;
size_t nextEvent = 0;
int eventSample = 0;
int pos = 0, loops = 0, length = 0;
int thread = -1, sema = -1, port = -1;
std::atomic<bool> ending{false};
std::atomic<bool> producedSignal{false};
std::atomic<bool> producedTimGM{false};

void resetNotes() {
  std::fill_n(notes, voices, Voice{});
  std::fill_n(channels, 16, Channel{});
}

void stopTrack() {
  if (active) active->playing = false;
  active.reset();
  activeBaked = nullptr;
  resetNotes();
}

uint32_t bent(int key, int bend) {
  if (bend == 8192) return step[key];
  return (uint32_t)(step[key] * std::pow(2.0, (bend - 8192) / (8192.0 * 6)));
}

void message(uint32_t msg) {
  int ch = msg & 15, type = msg & 0xf0, a = (msg >> 8) & 127, b = (msg >> 16) & 127;
  auto& c = channels[ch];
  if (type == 0x90 && b) {
    Voice* v = &notes[0];
    for (auto& note : notes) {
      if (!note.velocity) { v = &note; break; }
      if ((note.off && !v->off) || (note.off == v->off && note.age > v->age)) v = &note;
    }
    static constexpr int family[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 1, 4, 5, 3, 0, 4};
    *v = Voice{ch, a, b, ch == 9 ? (a < 37 ? 6 : 7) : family[c.program >> 3]};
    v->speed = ch == 9 && a < 37 ? step[37] : bent(a, c.bend);
  } else if (type == 0x80 || type == 0x90) {
    for (auto& n : notes) if (n.velocity && n.ch == ch && n.key == a) {
      n.held = c.pedal;
      n.off = !c.pedal;
    }
  } else if (type == 0xc0) {
    c.program = a;
  } else if (type == 0xe0) {
    c.bend = a | (b << 7);
    for (auto& n : notes) if (n.velocity && n.ch == ch && ch != 9) n.speed = bent(n.key, c.bend);
  } else if (type == 0xb0) {
    if (a == 7) c.volume = b;
    if (a == 11) c.expression = b;
    if (a == 64 || a == 121) {
      if (a == 121) {
        int program = c.program;
        c = Channel{};
        c.program = program;
        for (auto& n : notes) if (n.velocity && n.ch == ch) n.speed = step[n.key];
      }
      c.pedal = a == 64 && b >= 64;
      if (!c.pedal) for (auto& n : notes) if (n.ch == ch && n.held) n.held = false, n.off = true;
    }
    if (a == 120 || a == 123) {
      for (auto& n : notes) if (n.ch == ch) {
        if (a == 120) n.velocity = 0;
        else n.off = true;
      }
    }
  }
}

void updateEventTime() {
  eventSample = nextEvent < active->events.size()
    ? (int)(std::min(active->events[nextEvent].ms, 3600000.0) * (rate / 1000.0) + 0.5)
    : length;
}

void mixSpan(int32_t* sums, int count) {
  for (auto& n : notes) {
    if (!n.velocity) continue;
    const auto& c = channels[n.ch];
    const int gain = (n.velocity * c.volume * c.expression) >> 14;
    const bool drum = n.ch == 9;
    for (int i = 0; i < count; ++i) {
      ++n.age;
      if (drum && n.age > (n.shape == 6 ? 950 : 320)) n.off = true;
      if (n.off) n.envelope -= drum ? 15 : 4;
      else n.envelope = std::min(4096, n.envelope + 32);
      if (n.envelope <= 0) { n.velocity = 0; break; }
      int sample = wave[n.shape][n.phase >> 24];
      if (n.shape == 7) {
        noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
        sample = (int)(noise & 4095) - 2048;
      }
      n.phase += n.speed;
      sums[i] += ((sample * n.envelope) >> 12) * gain >> 7;
    }
  }
}

void mix(int16_t* out) {
  if (!active) { std::fill_n(out, block, 0); return; }
  int32_t sums[block]{};
  for (int i = 0; i < block;) {
    if (pos >= length) {
      if (loops > 0 && --loops == 0) { stopTrack(); break; }
      resetNotes();
      pos = 0;
      nextEvent = 0;
      updateEventTime();
    }
    while (eventSample <= pos) {
      message(active->events[nextEvent++].msg);
      updateEventTime();
    }
    const int count = std::min(block - i, std::min(length, eventSample) - pos);
    mixSpan(sums + i, count);
    i += count;
    pos += count;
  }
  for (int i = 0; i < block; ++i) out[i] = (int16_t)std::clamp<int32_t>(sums[i] * 2, -32768, 32767);
}

void resetBaked() {
  int frames = activeBaked->frames;
  if (loops == 1) frames += TimGM::tail;
  rendered.start(pack.data(*activeBaked), frames);
  pos = 0;
  length = frames * (rate / TimGM::rate);
}

void mixBaked(int16_t* out) {
  int i = 0;
  while (i < block) {
    if (pos == length) {
      if (loops > 0 && --loops == 0) { stopTrack(); break; }
      resetBaked();
    }
    int n = std::min(block - i, length - pos);
    for (int j = 0; j < n; ++j) out[i + j] = rendered.output();
    i += n;
    pos += n;
  }
  std::fill(out + i, out + block, 0);
}

int audioThread(SceSize, void*) {
  int i = 0;
  while (!ending) {
    sceKernelWaitSema(sema, 1, nullptr);
    const bool baked = activeBaked != nullptr;
    if (baked) mixBaked(pcm[i]); else mix(pcm[i]);
    sceKernelSignalSema(sema, 1);
    // CDRAM texture/audio buffers are uncached, so no explicit writeback is needed.
    if (sceAudioOutOutput(port, pcm[i]) < 0) {
      sceKernelDelayThread(10000);
    } else if (!producedSignal || (baked && !producedTimGM)) {
      if (std::any_of(pcm[i], pcm[i] + block, [](int16_t s) { return s != 0; })) {
        producedSignal = true;
        if (baked) producedTimGM = true;
      }
    }
    i ^= 1;
  }
  return 0;
}
}  // namespace

bool audioProducedSignal() { return producedSignal; }
bool audioProducedTimGM() { return producedTimGM; }

void shutdownAudio() {
  if (thread >= 0) {
    ending = true;
    sceKernelWaitThreadEnd(thread, nullptr, nullptr);
    sceKernelDeleteThread(thread);
    thread = -1;
  }
  stopTrack();
  if (port >= 0) { sceAudioOutReleasePort(port); port = -1; }
  if (sema >= 0) { sceKernelDeleteSema(sema); sema = -1; }
}

bool initAudio() {
  if (thread >= 0) return true;
  if (pack.load(Platform::dataDir + "/timgm.dat"))
    audioLog("TimGM6mb cache ready: " + std::to_string(pack.size()) + " bytes");
  else
    audioLog("No TimGM6mb cache: using MIDI fallback");
  for (int i = 0; i < waveSize; ++i) {
    double t = tau * i / waveSize;
    double s = std::sin(t), h2 = std::sin(2*t), h3 = std::sin(3*t), h5 = std::sin(5*t);
    const double shapes[waveCount] = {s*.75+h2*.18+h3*.07, s, s*.65+h3*.25+h5*.1,
                                     s*.6+h2*.24+h3*.16, s*.55+h2*.27+h3*.1+h5*.08,
                                     s*.7+h5*.3, s, 0};
    for (int j = 0; j < waveCount; ++j) wave[j][i] = (int16_t)(shapes[j] * 2300);
  }
  for (int i = 0; i < 128; ++i)
    step[i] = (uint32_t)(std::pow(2.0, (i - 69) / 12.0) * 440.0 * (4294967296.0 / rate));
  ending = false;
  sema = sceKernelCreateSema("GOW audio lock", 0, 1, 1, nullptr);
  if (sema >= 0)
    port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, block, rate, SCE_AUDIO_OUT_MODE_MONO);
  if (port >= 0)
    thread = sceKernelCreateThread("GOW audio", audioThread, kAudioPriority, kAudioStack, 0, 0, nullptr);
  if (thread >= 0) {
    if (sceKernelStartThread(thread, 0, nullptr) >= 0) return true;
    sceKernelDeleteThread(thread);
    thread = -1;
  }
  shutdownAudio();
  return false;
}

namespace gow {
void audioLog(const std::string& text) {
  if (FILE* f = std::fopen("ux0:data/gow-betrayal/gow-vita.log", "a")) {
    std::fprintf(f, "[audio] %s\n", text.c_str());
    std::fclose(f);
  }
}

bool Player::isMidi() const {
  return mime.str() == u"audio/midi";
}
void Player::prefetch() {
  if (!seq_ && data != nullptr && isMidi()) {
    baked_ = pack.find(data);
    seq_ = baked_ ? std::make_shared<MidiSequence>() : parseMidi(data);
    if (!baked_ && seq_->events.empty()) audioLog("Unrecognized MIDI track");
  }
}
void Player::start() {
  prefetch();
  if (!seq_ || (!baked_ && seq_->events.empty()) || thread < 0) return;
  sceKernelWaitSema(sema, 1, nullptr);
  stopTrack();
  active = seq_;
  active->playing = true;
  loops = loops_ < 0 ? -1 : std::max(1, loops_);
  activeBaked = baked_;
  if (activeBaked) {
    resetBaked();
  } else {
    nextEvent = 0;
    pos = 0;
    length = (int)(std::min(active->lengthMs, 3600000.0) * (rate / 1000.0)) + rate / 20;
    updateEventTime();
  }
  state_ = 400;
  sceKernelSignalSema(sema, 1);
}
void Player::stop() {
  if (seq_ && thread >= 0) {
    sceKernelWaitSema(sema, 1, nullptr);
    if (active == seq_) stopTrack();
    sceKernelSignalSema(sema, 1);
  }
  state_ = 300;
}
void Player::close() {
  // The generated SoundPlayer unloads the sole owner immediately after close().
  stop();
  delete this;
}
int Player::getState() {
  if (state_ == 400 && (!seq_ || !seq_->playing)) state_ = 300;
  return state_;
}
void Player::pumpLoops() {}
}  // namespace gow
