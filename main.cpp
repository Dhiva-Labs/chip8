#include <SDL.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "cpu.h"
#include "display.h"
#include "input.h"
#include "memory.h"
#include "rom_loader.h"

namespace {

struct Options {
  std::string rom_path;
  int ips = 540;  // instructions per second, ~COSMAC VIP speed
  int scale = 12;
  bool modern = false;
  bool mute = false;
  long frames = -1;             // exit after N 60Hz frames; -1 = run until quit
  std::string screenshot_path;  // save a BMP just before exiting
};

void print_usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s [options] <rom.ch8>\n"
               "  --ips N            instructions per second (default 540)\n"
               "  --scale N          window scale factor (default 12)\n"
               "  --profile P        quirk profile: vip (default) or modern\n"
               "  --mute             disable the beeper\n"
               "  --frames N         exit after N frames (for testing)\n"
               "  --screenshot PATH  save a BMP screenshot before exiting\n"
               "keys: 1234/QWER/ASDF/ZXCV = keypad, Space = pause, Esc = quit\n",
               argv0);
}

bool parse_args(int argc, char** argv, Options& opts) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next_value = [&]() -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: %s requires a value\n", arg.c_str());
        return nullptr;
      }
      return argv[++i];
    };
    if (arg == "--ips") {
      const char* v = next_value();
      if (!v) return false;
      opts.ips = std::atoi(v);
    } else if (arg == "--scale") {
      const char* v = next_value();
      if (!v) return false;
      opts.scale = std::atoi(v);
    } else if (arg == "--profile") {
      const char* v = next_value();
      if (!v) return false;
      if (std::strcmp(v, "vip") == 0) {
        opts.modern = false;
      } else if (std::strcmp(v, "modern") == 0) {
        opts.modern = true;
      } else {
        std::fprintf(stderr, "error: unknown profile '%s'\n", v);
        return false;
      }
    } else if (arg == "--mute") {
      opts.mute = true;
    } else if (arg == "--frames") {
      const char* v = next_value();
      if (!v) return false;
      opts.frames = std::atol(v);
    } else if (arg == "--screenshot") {
      const char* v = next_value();
      if (!v) return false;
      opts.screenshot_path = v;
    } else if (arg == "--help" || arg == "-h") {
      return false;
    } else if (!arg.empty() && arg[0] == '-') {
      std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
      return false;
    } else {
      opts.rom_path = arg;
    }
  }
  if (opts.rom_path.empty()) return false;
  if (opts.ips < 60) opts.ips = 60;
  if (opts.scale < 1) opts.scale = 1;
  return true;
}

void apply_modern_profile(chip8::Quirks& q) {
  q.vf_reset = false;
  q.increment_i = false;
  q.display_wait = false;
  q.shift_vy = false;
  q.jump_v0 = false;
  // clipping stays on: SUPER-CHIP-style interpreters clip too
}

// 440Hz square wave while the sound timer is running.
class Beeper {
 public:
  Beeper() {
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = &Beeper::fill;
    want.userdata = this;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
    if (device_ != 0) SDL_PauseAudioDevice(device_, 0);
  }
  ~Beeper() {
    if (device_ != 0) SDL_CloseAudioDevice(device_);
  }
  Beeper(const Beeper&) = delete;
  Beeper& operator=(const Beeper&) = delete;

  void set_active(bool on) { active_.store(on, std::memory_order_relaxed); }

 private:
  static void fill(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<Beeper*>(userdata);
    auto* out = reinterpret_cast<Sint16*>(stream);
    const int count = len / static_cast<int>(sizeof(Sint16));
    const bool on = self->active_.load(std::memory_order_relaxed);
    const int period = self->spec_.freq / 440;
    for (int i = 0; i < count; ++i) {
      if (on) {
        out[i] = self->phase_ < period / 2 ? 3000 : -3000;
        self->phase_ = (self->phase_ + 1) % period;
      } else {
        out[i] = 0;
      }
    }
  }

  SDL_AudioDeviceID device_ = 0;
  SDL_AudioSpec spec_{};
  std::atomic<bool> active_{false};
  int phase_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
  Options opts;
  if (!parse_args(argc, argv, opts)) {
    print_usage(argv[0]);
    return 2;
  }

  std::vector<std::uint8_t> rom;
  try {
    rom = chip8::load_rom_file(opts.rom_path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }

  Uint32 sdl_flags = SDL_INIT_VIDEO;
  if (!opts.mute) sdl_flags |= SDL_INIT_AUDIO;
  if (SDL_Init(sdl_flags) != 0) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  int rc = 0;
  try {
    chip8::Memory memory;
    chip8::Display display;
    chip8::Keypad keypad;
    memory.load_rom(rom.data(), rom.size());
    chip8::Cpu cpu(memory, display, keypad,
                   static_cast<std::uint32_t>(SDL_GetPerformanceCounter()));
    if (opts.modern) apply_modern_profile(cpu.quirks());

    chip8::Renderer renderer(opts.scale, "chip8 - " + opts.rom_path);
    Beeper beeper;

    bool running = true;
    bool paused = false;
    long frames_done = 0;
    double timer_acc = 0.0;
    double instr_acc = 0.0;
    std::uint64_t last = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    constexpr double kTimerPeriod = 1.0 / 60.0;

    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_QUIT:
            running = false;
            break;
          case SDL_KEYDOWN:
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
              running = false;
              break;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_SPACE && event.key.repeat == 0) {
              paused = !paused;
              break;
            }
            [[fallthrough]];
          case SDL_KEYUP:
            if (event.key.repeat == 0) {
              if (const auto key = chip8::map_sdl_scancode(event.key.keysym.scancode))
                keypad.set_key(*key, event.type == SDL_KEYDOWN);
            }
            break;
          default:
            break;
        }
      }

      const std::uint64_t now = SDL_GetPerformanceCounter();
      double dt = static_cast<double>(now - last) / freq;
      last = now;
      if (dt > 0.25) dt = 0.25;  // ignore stalls (window drag, debugger)

      if (!paused) {
        timer_acc += dt;
        instr_acc += dt * opts.ips;

        // Timers run off wall-clock time at 60Hz, independent of how many
        // instructions execute.
        bool ticked = false;
        while (timer_acc >= kTimerPeriod) {
          timer_acc -= kTimerPeriod;
          cpu.tick_timers();
          ticked = true;
          ++frames_done;
        }
        beeper.set_active(cpu.sound_timer() > 0);

        while (instr_acc >= 1.0) {
          if (cpu.waiting_vblank()) {
            // COSMAC VIP display wait: remaining cycles this frame are lost.
            instr_acc = 0.0;
            break;
          }
          cpu.step();
          instr_acc -= 1.0;
        }

        if (ticked || display.take_dirty()) renderer.present(display);
      }

      if (opts.frames >= 0 && frames_done >= opts.frames) running = false;
      SDL_Delay(1);
    }

    if (!opts.screenshot_path.empty()) {
      renderer.present(display);
      if (!renderer.save_screenshot(opts.screenshot_path))
        std::fprintf(stderr, "warning: screenshot failed: %s\n", SDL_GetError());
    }

    if (const auto op = cpu.last_unknown_opcode())
      std::fprintf(stderr, "warning: unknown opcode 0x%04X encountered\n", *op);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    rc = 1;
  }

  SDL_Quit();
  return rc;
}
