#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace chip8 {

// 16-key hex keypad. FX0A ("wait for key") follows COSMAC VIP semantics: the
// instruction completes when a key is released, not when it is first pressed.
class Keypad {
 public:
  void set_key(std::uint8_t key, bool down);
  bool is_down(std::uint8_t key) const { return down_[key & 0xF]; }

  // FX0A support: arm the wait, then poll for a completed press+release.
  void begin_wait();
  std::optional<std::uint8_t> take_wait_release();

 private:
  std::array<bool, 16> down_{};
  std::optional<std::uint8_t> wait_release_;
  bool waiting_ = false;
};

// Maps an SDL scancode to a CHIP-8 key (COSMAC layout on 1234/QWER/ASDF/ZXCV).
// Takes the scancode as int so this header stays SDL-free.
std::optional<std::uint8_t> map_sdl_scancode(int scancode);

}  // namespace chip8
