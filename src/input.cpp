#include "input.h"

#include <SDL.h>

namespace chip8 {

void Keypad::set_key(std::uint8_t key, bool down) {
  key &= 0xF;
  if (waiting_ && down_[key] && !down) {
    wait_release_ = key;
    waiting_ = false;
  }
  down_[key] = down;
}

void Keypad::begin_wait() {
  waiting_ = true;
  wait_release_.reset();
}

std::optional<std::uint8_t> Keypad::take_wait_release() {
  const auto key = wait_release_;
  wait_release_.reset();
  return key;
}

std::optional<std::uint8_t> map_sdl_scancode(int scancode) {
  switch (scancode) {
    case SDL_SCANCODE_1: return 0x1;
    case SDL_SCANCODE_2: return 0x2;
    case SDL_SCANCODE_3: return 0x3;
    case SDL_SCANCODE_4: return 0xC;
    case SDL_SCANCODE_Q: return 0x4;
    case SDL_SCANCODE_W: return 0x5;
    case SDL_SCANCODE_E: return 0x6;
    case SDL_SCANCODE_R: return 0xD;
    case SDL_SCANCODE_A: return 0x7;
    case SDL_SCANCODE_S: return 0x8;
    case SDL_SCANCODE_D: return 0x9;
    case SDL_SCANCODE_F: return 0xE;
    case SDL_SCANCODE_Z: return 0xA;
    case SDL_SCANCODE_X: return 0x0;
    case SDL_SCANCODE_C: return 0xB;
    case SDL_SCANCODE_V: return 0xF;
    default: return std::nullopt;
  }
}

}  // namespace chip8
