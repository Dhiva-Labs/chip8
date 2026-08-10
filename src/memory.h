#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace chip8 {

// 4KB RAM. Font sits at 0x050-0x09F, ROMs load at 0x200. All accesses are
// masked to 12 bits, so out-of-range addresses wrap instead of faulting.
class Memory {
 public:
  static constexpr std::size_t kSize = 4096;
  static constexpr std::uint16_t kFontBase = 0x050;
  static constexpr std::uint16_t kRomBase = 0x200;

  Memory();

  std::uint8_t read(std::uint16_t addr) const { return bytes_[addr & 0x0FFF]; }
  void write(std::uint16_t addr, std::uint8_t value) { bytes_[addr & 0x0FFF] = value; }

  // Copies a ROM image to 0x200. Returns false if it does not fit.
  bool load_rom(const std::uint8_t* data, std::size_t size);

  void reset();

 private:
  std::array<std::uint8_t, kSize> bytes_{};
};

}  // namespace chip8
