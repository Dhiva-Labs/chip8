#include "rom_loader.h"

#include <fstream>
#include <stdexcept>

#include "memory.h"

namespace chip8 {

std::vector<std::uint8_t> load_rom_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) throw std::runtime_error("cannot open ROM: " + path);
  const auto size = static_cast<std::size_t>(file.tellg());
  if (size == 0) throw std::runtime_error("ROM is empty: " + path);
  if (size > Memory::kSize - Memory::kRomBase)
    throw std::runtime_error("ROM too large (" + std::to_string(size) + " bytes, max " +
                             std::to_string(Memory::kSize - Memory::kRomBase) + "): " + path);
  std::vector<std::uint8_t> data(size);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
  if (!file) throw std::runtime_error("failed to read ROM: " + path);
  return data;
}

}  // namespace chip8
