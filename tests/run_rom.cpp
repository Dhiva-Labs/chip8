// Headless test runner: executes a ROM for a fixed number of simulated 60Hz
// frames and dumps the framebuffer as ASCII art (optionally a BMP). This is
// how the Timendus suite results are checked without opening a window.
//
//   chip8_run_rom rom.ch8 [--frames N] [--ips N] [--profile vip|modern]
//                 [--seed N] [--poke ADDR=VAL]... [--key K:DOWN:UP]...
//                 [--bmp PATH]
//
// --poke writes a byte after ROM load (the Timendus ROMs read 0x1FF to skip
// their menus). --key presses hex key K at frame DOWN and releases it at
// frame UP.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "cpu.h"
#include "display.h"
#include "input.h"
#include "memory.h"
#include "rom_loader.h"

namespace {

struct KeyEvent {
  long frame = 0;
  std::uint8_t key = 0;
  bool down = false;
};

struct Options {
  std::string rom_path;
  long frames = 480;
  int ips = 540;
  bool modern = false;
  std::uint32_t seed = 0x2A2A2A2A;
  std::vector<std::pair<std::uint16_t, std::uint8_t>> pokes;
  std::vector<KeyEvent> keys;
  std::string bmp_path;
};

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
    if (arg == "--frames") {
      const char* v = next_value();
      if (!v) return false;
      opts.frames = std::atol(v);
    } else if (arg == "--ips") {
      const char* v = next_value();
      if (!v) return false;
      opts.ips = std::atoi(v);
    } else if (arg == "--profile") {
      const char* v = next_value();
      if (!v) return false;
      if (std::strcmp(v, "modern") == 0) opts.modern = true;
      else if (std::strcmp(v, "vip") != 0) {
        std::fprintf(stderr, "error: unknown profile '%s'\n", v);
        return false;
      }
    } else if (arg == "--seed") {
      const char* v = next_value();
      if (!v) return false;
      opts.seed = static_cast<std::uint32_t>(std::strtoul(v, nullptr, 0));
    } else if (arg == "--poke") {
      const char* v = next_value();
      if (!v) return false;
      int addr = 0, value = 0;
      if (std::sscanf(v, "%i=%i", &addr, &value) != 2) {
        std::fprintf(stderr, "error: bad --poke '%s' (want ADDR=VAL)\n", v);
        return false;
      }
      opts.pokes.emplace_back(static_cast<std::uint16_t>(addr),
                              static_cast<std::uint8_t>(value));
    } else if (arg == "--key") {
      const char* v = next_value();
      if (!v) return false;
      unsigned key = 0;
      long down = 0, up = 0;
      if (std::sscanf(v, "%x:%ld:%ld", &key, &down, &up) != 3 || key > 0xF || up <= down) {
        std::fprintf(stderr, "error: bad --key '%s' (want K:DOWN:UP)\n", v);
        return false;
      }
      opts.keys.push_back({down, static_cast<std::uint8_t>(key), true});
      opts.keys.push_back({up, static_cast<std::uint8_t>(key), false});
    } else if (arg == "--bmp") {
      const char* v = next_value();
      if (!v) return false;
      opts.bmp_path = v;
    } else if (!arg.empty() && arg[0] == '-') {
      std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
      return false;
    } else {
      opts.rom_path = arg;
    }
  }
  return !opts.rom_path.empty();
}

// Minimal 24-bit BMP writer so the runner has no SDL/video dependency.
bool write_bmp(const std::string& path, const chip8::Display& display, int scale) {
  const int w = chip8::Display::kWidth * scale;
  const int h = chip8::Display::kHeight * scale;
  const int row_bytes = w * 3;
  const int pad = (4 - row_bytes % 4) % 4;
  const std::uint32_t data_size = static_cast<std::uint32_t>((row_bytes + pad) * h);
  const std::uint32_t file_size = 54 + data_size;

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::uint8_t header[54] = {'B', 'M'};
  const auto put32 = [&header](int off, std::uint32_t v) {
    header[off] = v & 0xFF;
    header[off + 1] = (v >> 8) & 0xFF;
    header[off + 2] = (v >> 16) & 0xFF;
    header[off + 3] = (v >> 24) & 0xFF;
  };
  put32(2, file_size);
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<std::uint32_t>(w));
  put32(22, static_cast<std::uint32_t>(h));
  header[26] = 1;
  header[28] = 24;
  put32(34, data_size);
  std::fwrite(header, 1, sizeof(header), f);

  std::vector<std::uint8_t> line(static_cast<std::size_t>(row_bytes + pad), 0);
  for (int y = h - 1; y >= 0; --y) {  // BMP rows are bottom-up
    for (int x = 0; x < w; ++x) {
      const bool on = display.pixel(x / scale, y / scale);
      line[x * 3 + 0] = on ? 0xE8 : 0x18;
      line[x * 3 + 1] = on ? 0xF0 : 0x14;
      line[x * 3 + 2] = on ? 0xE8 : 0x10;
    }
    std::fwrite(line.data(), 1, line.size(), f);
  }
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opts;
  if (!parse_args(argc, argv, opts)) {
    std::fprintf(stderr,
                 "usage: %s <rom.ch8> [--frames N] [--ips N] [--profile vip|modern]\n"
                 "       [--seed N] [--poke ADDR=VAL]... [--key K:DOWN:UP]... [--bmp PATH]\n",
                 argv[0]);
    return 2;
  }

  try {
    const auto rom = chip8::load_rom_file(opts.rom_path);

    chip8::Memory memory;
    chip8::Display display;
    chip8::Keypad keypad;
    memory.load_rom(rom.data(), rom.size());
    for (const auto& [addr, value] : opts.pokes) memory.write(addr, value);

    chip8::Cpu cpu(memory, display, keypad, opts.seed);
    if (opts.modern) {
      auto& q = cpu.quirks();
      q.vf_reset = false;
      q.increment_i = false;
      q.display_wait = false;
      q.shift_vy = false;
      q.jump_v0 = false;
    }

    const double per_frame = opts.ips / 60.0;
    double acc = 0.0;
    for (long frame = 0; frame < opts.frames; ++frame) {
      for (const auto& ev : opts.keys)
        if (ev.frame == frame) keypad.set_key(ev.key, ev.down);
      acc += per_frame;
      while (acc >= 1.0) {
        if (cpu.waiting_vblank()) {
          acc = 0.0;
          break;
        }
        cpu.step();
        acc -= 1.0;
      }
      cpu.tick_timers();
    }

    std::printf("%s | frames=%ld ips=%d profile=%s | PC=0x%03X I=0x%03X DT=%u ST=%u\n",
                opts.rom_path.c_str(), opts.frames, opts.ips, opts.modern ? "modern" : "vip",
                cpu.pc(), cpu.index(), cpu.delay_timer(), cpu.sound_timer());
    if (const auto op = cpu.last_unknown_opcode())
      std::printf("WARNING: unknown opcode 0x%04X encountered\n", *op);

    std::string border(chip8::Display::kWidth * 2, '-');
    std::printf("+%s+\n", border.c_str());
    for (int y = 0; y < chip8::Display::kHeight; ++y) {
      std::string line;
      line.reserve(chip8::Display::kWidth * 2);
      for (int x = 0; x < chip8::Display::kWidth; ++x)
        line += display.pixel(x, y) ? "##" : "  ";
      std::printf("|%s|\n", line.c_str());
    }
    std::printf("+%s+\n", border.c_str());

    if (!opts.bmp_path.empty() && !write_bmp(opts.bmp_path, display, 8)) {
      std::fprintf(stderr, "error: failed to write %s\n", opts.bmp_path.c_str());
      return 1;
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
