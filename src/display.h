#pragma once

#include <array>
#include <cstdint>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace chip8 {

// 64x32 monochrome framebuffer. Sprites are XOR-drawn; erasing a lit pixel
// reports a collision (used for VF).
class Display {
 public:
  static constexpr int kWidth = 64;
  static constexpr int kHeight = 32;

  void clear();

  // Draws an n-row sprite at (x, y). Start coordinates wrap modulo the screen
  // size; pixels past the right/bottom edge clip when wrap is false (COSMAC
  // VIP behavior) and wrap around when wrap is true.
  bool draw_sprite(std::uint8_t x, std::uint8_t y, const std::uint8_t* rows, int n, bool wrap);

  bool pixel(int x, int y) const { return fb_[y * kWidth + x] != 0; }
  const std::array<std::uint8_t, kWidth * kHeight>& buffer() const { return fb_; }

  // True once since the last call if the framebuffer changed.
  bool take_dirty();

 private:
  std::array<std::uint8_t, kWidth * kHeight> fb_{};
  bool dirty_ = true;
};

// SDL2 window/texture that blits the framebuffer at an integer scale.
class Renderer {
 public:
  Renderer(int scale, const std::string& title);  // throws std::runtime_error
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void present(const Display& display);
  bool save_screenshot(const std::string& path);

 private:
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
};

}  // namespace chip8
