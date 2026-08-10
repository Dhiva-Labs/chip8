#include "display.h"

#include <SDL.h>

#include <stdexcept>
#include <vector>

namespace chip8 {

void Display::clear() {
  fb_.fill(0);
  dirty_ = true;
}

bool Display::draw_sprite(std::uint8_t x, std::uint8_t y, const std::uint8_t* rows, int n,
                          bool wrap) {
  bool collision = false;
  const int start_x = x % kWidth;
  const int start_y = y % kHeight;
  for (int row = 0; row < n; ++row) {
    int py = start_y + row;
    if (py >= kHeight) {
      if (!wrap) break;
      py %= kHeight;
    }
    const std::uint8_t bits = rows[row];
    for (int bit = 0; bit < 8; ++bit) {
      if (!(bits & (0x80u >> bit))) continue;
      int px = start_x + bit;
      if (px >= kWidth) {
        if (!wrap) break;
        px %= kWidth;
      }
      std::uint8_t& cell = fb_[py * kWidth + px];
      if (cell) {
        cell = 0;
        collision = true;
      } else {
        cell = 1;
      }
    }
  }
  dirty_ = true;
  return collision;
}

bool Display::take_dirty() {
  const bool was = dirty_;
  dirty_ = false;
  return was;
}

namespace {
constexpr std::uint32_t kOnColor = 0xFFE8F0E8;
constexpr std::uint32_t kOffColor = 0xFF101418;
}  // namespace

Renderer::Renderer(int scale, const std::string& title) {
  window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             Display::kWidth * scale, Display::kHeight * scale, SDL_WINDOW_SHOWN);
  if (!window_) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, 0);
  if (!renderer_) throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                               Display::kWidth, Display::kHeight);
  if (!texture_) throw std::runtime_error(std::string("SDL_CreateTexture: ") + SDL_GetError());
}

Renderer::~Renderer() {
  if (texture_) SDL_DestroyTexture(texture_);
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
}

void Renderer::present(const Display& display) {
  std::array<std::uint32_t, Display::kWidth * Display::kHeight> pixels;
  const auto& fb = display.buffer();
  for (int i = 0; i < Display::kWidth * Display::kHeight; ++i)
    pixels[i] = fb[i] ? kOnColor : kOffColor;
  SDL_UpdateTexture(texture_, nullptr, pixels.data(), Display::kWidth * sizeof(std::uint32_t));
  SDL_RenderClear(renderer_);
  SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
  SDL_RenderPresent(renderer_);
}

bool Renderer::save_screenshot(const std::string& path) {
  int w = 0, h = 0;
  if (SDL_GetRendererOutputSize(renderer_, &w, &h) != 0) return false;
  std::vector<std::uint8_t> data(static_cast<std::size_t>(w) * h * 4);
  if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, data.data(), w * 4) != 0)
    return false;
  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormatFrom(data.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ARGB8888);
  if (!surface) return false;
  const bool ok = SDL_SaveBMP(surface, path.c_str()) == 0;
  SDL_FreeSurface(surface);
  return ok;
}

}  // namespace chip8
