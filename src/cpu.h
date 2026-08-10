#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>

#include "display.h"
#include "input.h"
#include "memory.h"

namespace chip8 {

// Quirk switches, named for the behavior when true. Defaults are the original
// COSMAC VIP interpreter; see the README's "Quirks" section.
struct Quirks {
  bool vf_reset = true;      // 8XY1/8XY2/8XY3 set VF to 0
  bool increment_i = true;   // FX55/FX65 leave I = I + X + 1
  bool display_wait = true;  // DXYN halts until the next 60Hz tick
  bool clipping = true;      // sprites clip at the right/bottom edge instead of wrapping
  bool shift_vy = true;      // 8XY6/8XYE shift VY into VX (false: shift VX in place)
  bool jump_v0 = true;       // BNNN jumps to NNN + V0 (false: BXNN, NNN + VX)
};

class Cpu {
 public:
  Cpu(Memory& memory, Display& display, Keypad& keypad, std::uint32_t rng_seed);

  // Fetch/decode/execute one instruction. No-op while halted on DXYN's
  // display wait; callers should end the current batch when waiting_vblank().
  void step();

  // 60Hz tick: decrements delay/sound timers and releases the display wait.
  void tick_timers();

  bool waiting_vblank() const { return vblank_wait_; }
  std::uint8_t delay_timer() const { return delay_; }
  std::uint8_t sound_timer() const { return sound_; }
  std::uint16_t pc() const { return pc_; }
  std::uint16_t index() const { return i_; }
  Quirks& quirks() { return quirks_; }

  std::optional<std::uint16_t> last_unknown_opcode() const { return unknown_opcode_; }

 private:
  using OpFn = void (Cpu::*)(std::uint16_t);
  static const std::array<OpFn, 16> kDispatch;

  void op_0(std::uint16_t op);
  void op_1(std::uint16_t op);
  void op_2(std::uint16_t op);
  void op_3(std::uint16_t op);
  void op_4(std::uint16_t op);
  void op_5(std::uint16_t op);
  void op_6(std::uint16_t op);
  void op_7(std::uint16_t op);
  void op_8(std::uint16_t op);
  void op_9(std::uint16_t op);
  void op_A(std::uint16_t op);
  void op_B(std::uint16_t op);
  void op_C(std::uint16_t op);
  void op_D(std::uint16_t op);
  void op_E(std::uint16_t op);
  void op_F(std::uint16_t op);

  void unknown(std::uint16_t op) { unknown_opcode_ = op; }

  Memory& mem_;
  Display& display_;
  Keypad& keypad_;
  Quirks quirks_;

  std::array<std::uint8_t, 16> v_{};
  std::uint16_t i_ = 0;
  std::uint16_t pc_ = Memory::kRomBase;
  std::uint8_t sp_ = 0;
  std::array<std::uint16_t, 16> stack_{};
  std::uint8_t delay_ = 0;
  std::uint8_t sound_ = 0;

  bool vblank_wait_ = false;
  bool vblank_seen_ = true;  // one-draw-per-frame quota, refilled by tick_timers
  bool key_wait_ = false;

  std::mt19937 rng_;
  std::optional<std::uint16_t> unknown_opcode_;
};

}  // namespace chip8
