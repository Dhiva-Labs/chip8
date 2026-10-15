#include "cpu.h"

namespace chip8 {

const std::array<Cpu::OpFn, 16> Cpu::kDispatch = {
    &Cpu::op_0, &Cpu::op_1, &Cpu::op_2, &Cpu::op_3, &Cpu::op_4, &Cpu::op_5,
    &Cpu::op_6, &Cpu::op_7, &Cpu::op_8, &Cpu::op_9, &Cpu::op_A, &Cpu::op_B,
    &Cpu::op_C, &Cpu::op_D, &Cpu::op_E, &Cpu::op_F,
};

Cpu::Cpu(Memory& memory, Display& display, Keypad& keypad, std::uint32_t rng_seed)
    : mem_(memory), display_(display), keypad_(keypad), rng_(rng_seed) {}

void Cpu::step() {
  if (vblank_wait_) return;
  const auto op =
      static_cast<std::uint16_t>((mem_.read(pc_) << 8) | mem_.read(pc_ + 1));
  pc_ = (pc_ + 2) & 0x0FFF;
  (this->*kDispatch[(op >> 12) & 0xF])(op);
}

void Cpu::tick_timers() {
  if (delay_ > 0) --delay_;
  if (sound_ > 0) --sound_;
  vblank_wait_ = false;
  vblank_seen_ = true;
}

void Cpu::op_0(std::uint16_t op) {
  switch (op & 0x0FFF) {
    case 0x0E0:  // CLS
      display_.clear();
      break;
    case 0x0EE:  // RET
      if (sp_ == 0) {
        unknown(op);
        break;
      }
      pc_ = stack_[--sp_];
      break;
    default:  // 0NNN machine-code call: ignored
      break;
  }
}

void Cpu::op_1(std::uint16_t op) { pc_ = op & 0x0FFF; }

void Cpu::op_2(std::uint16_t op) {
  if (sp_ >= stack_.size()) {
    unknown(op);
    return;
  }
  stack_[sp_++] = pc_;
  pc_ = op & 0x0FFF;
}

void Cpu::op_3(std::uint16_t op) {
  if (v_[(op >> 8) & 0xF] == (op & 0xFF)) pc_ = (pc_ + 2) & 0x0FFF;
}

void Cpu::op_4(std::uint16_t op) {
  if (v_[(op >> 8) & 0xF] != (op & 0xFF)) pc_ = (pc_ + 2) & 0x0FFF;
}

void Cpu::op_5(std::uint16_t op) {
  if ((op & 0xF) != 0) {
    unknown(op);
    return;
  }
  if (v_[(op >> 8) & 0xF] == v_[(op >> 4) & 0xF]) pc_ = (pc_ + 2) & 0x0FFF;
}

void Cpu::op_6(std::uint16_t op) {
  v_[(op >> 8) & 0xF] = static_cast<std::uint8_t>(op & 0xFF);
}

void Cpu::op_7(std::uint16_t op) {
  auto& vx = v_[(op >> 8) & 0xF];
  vx = static_cast<std::uint8_t>(vx + (op & 0xFF));
}

void Cpu::op_8(std::uint16_t op) {
  const int x = (op >> 8) & 0xF;
  const int y = (op >> 4) & 0xF;
  // Arithmetic ops write the result before the flag so that VF holds the
  // flag, not the result, when X is F (the flags test checks this).
  switch (op & 0xF) {
    case 0x0:
      v_[x] = v_[y];
      break;
    case 0x1:
      v_[x] |= v_[y];
      if (quirks_.vf_reset) v_[0xF] = 0;
      break;
    case 0x2:
      v_[x] &= v_[y];
      if (quirks_.vf_reset) v_[0xF] = 0;
      break;
    case 0x3:
      v_[x] ^= v_[y];
      if (quirks_.vf_reset) v_[0xF] = 0;
      break;
    case 0x4: {
      const int sum = v_[x] + v_[y];
      v_[x] = static_cast<std::uint8_t>(sum);
      v_[0xF] = sum > 0xFF ? 1 : 0;
      break;
    }
    case 0x5: {
      const std::uint8_t flag = v_[x] >= v_[y] ? 1 : 0;
      v_[x] = static_cast<std::uint8_t>(v_[x] - v_[y]);
      v_[0xF] = flag;
      break;
    }
    case 0x6: {
      const std::uint8_t src = quirks_.shift_vy ? v_[y] : v_[x];
      v_[x] = src >> 1;
      v_[0xF] = src & 0x1;
      break;
    }
    case 0x7: {
      const std::uint8_t flag = v_[y] >= v_[x] ? 1 : 0;
      v_[x] = static_cast<std::uint8_t>(v_[y] - v_[x]);
      v_[0xF] = flag;
      break;
    }
    case 0xE: {
      const std::uint8_t src = quirks_.shift_vy ? v_[y] : v_[x];
      v_[x] = static_cast<std::uint8_t>(src << 1);
      v_[0xF] = (src >> 7) & 0x1;
      break;
    }
    default:
      unknown(op);
      break;
  }
}

void Cpu::op_9(std::uint16_t op) {
  if ((op & 0xF) != 0) {
    unknown(op);
    return;
  }
  if (v_[(op >> 8) & 0xF] != v_[(op >> 4) & 0xF]) pc_ = (pc_ + 2) & 0x0FFF;
}

void Cpu::op_A(std::uint16_t op) { i_ = op & 0x0FFF; }

void Cpu::op_B(std::uint16_t op) {
  const std::uint16_t nnn = op & 0x0FFF;
  const std::uint8_t offset = quirks_.jump_v0 ? v_[0x0] : v_[(op >> 8) & 0xF];
  pc_ = (nnn + offset) & 0x0FFF;
}

void Cpu::op_C(std::uint16_t op) {
  std::uniform_int_distribution<int> dist(0, 255);
  v_[(op >> 8) & 0xF] = static_cast<std::uint8_t>(dist(rng_) & (op & 0xFF));
}

void Cpu::op_D(std::uint16_t op) {
  // COSMAC VIP display wait: sprites are drawn during the 60Hz display
  // interrupt, so each frame allows at most one draw and the wait happens
  // BEFORE drawing. Code following the draw still runs in the same frame,
  // which the Timendus quirks test measures (halting after the draw instead
  // reads as "SLOW").
  if (quirks_.display_wait) {
    if (!vblank_seen_) {
      pc_ = (pc_ - 2) & 0x0FFF;  // retry this DXYN after the next tick
      vblank_wait_ = true;
      return;
    }
    vblank_seen_ = false;
  }
  const std::uint8_t x = v_[(op >> 8) & 0xF];
  const std::uint8_t y = v_[(op >> 4) & 0xF];
  const int n = op & 0xF;
  std::array<std::uint8_t, 15> rows{};
  for (int r = 0; r < n; ++r) rows[r] = mem_.read(static_cast<std::uint16_t>(i_ + r));
  v_[0xF] = display_.draw_sprite(x, y, rows.data(), n, !quirks_.clipping) ? 1 : 0;
}

void Cpu::op_E(std::uint16_t op) {
  const std::uint8_t key = v_[(op >> 8) & 0xF] & 0xF;
  switch (op & 0xFF) {
    case 0x9E:  // SKP
      if (keypad_.is_down(key)) pc_ = (pc_ + 2) & 0x0FFF;
      break;
    case 0xA1:  // SKNP
      if (!keypad_.is_down(key)) pc_ = (pc_ + 2) & 0x0FFF;
      break;
    default:
      unknown(op);
      break;
  }
}

void Cpu::op_F(std::uint16_t op) {
  const int x = (op >> 8) & 0xF;
  switch (op & 0xFF) {
    case 0x07:
      v_[x] = delay_;
      break;
    case 0x0A:  // wait for key: completes on release (COSMAC VIP semantics)
      if (!key_wait_) {
        key_wait_ = true;
        keypad_.begin_wait();
      }
      if (const auto key = keypad_.take_wait_release()) {
        v_[x] = *key;
        key_wait_ = false;
      } else {
        pc_ = (pc_ - 2) & 0x0FFF;
      }
      break;
    case 0x15:
      delay_ = v_[x];
      break;
    case 0x18:
      sound_ = v_[x];
      break;
    case 0x1E:
      i_ = static_cast<std::uint16_t>(i_ + v_[x]);
      break;
    case 0x29:
      i_ = static_cast<std::uint16_t>(Memory::kFontBase + (v_[x] & 0xF) * 5);
      break;
    case 0x33: {
      const std::uint8_t value = v_[x];
      mem_.write(i_, value / 100);
      mem_.write(i_ + 1, (value / 10) % 10);
      mem_.write(i_ + 2, value % 10);
      break;
    }
    case 0x55:
      for (int r = 0; r <= x; ++r) mem_.write(static_cast<std::uint16_t>(i_ + r), v_[r]);
      if (quirks_.increment_i) i_ = static_cast<std::uint16_t>(i_ + x + 1);
      break;
    case 0x65:
      for (int r = 0; r <= x; ++r) v_[r] = mem_.read(static_cast<std::uint16_t>(i_ + r));
      if (quirks_.increment_i) i_ = static_cast<std::uint16_t>(i_ + x + 1);
      break;
    default:
      unknown(op);
      break;
  }
}

}  // namespace chip8
