#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chip8 {

// Reads a ROM file. Throws std::runtime_error on I/O failure or if the image
// cannot fit in memory (larger than 4096 - 0x200 bytes).
std::vector<std::uint8_t> load_rom_file(const std::string& path);

}  // namespace chip8
