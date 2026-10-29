#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome::panasonic {

// Temperature
static constexpr uint8_t PANASONIC_TEMP_MIN = 16;  // Celsius
static constexpr uint8_t PANASONIC_TEMP_MAX = 30;  // Celsius

// Fan speed sets offered to the climate entity
enum SetFanMode : uint8_t {
  PANASONIC_FAN_3L = 0,  // Low, medium, high + auto
  PANASONIC_FAN_5L,      // Quiet, low, middle, medium, high + auto
};

// Fixed horizontal vane positions (low nibble of byte 17)
enum HorizontalDirection : uint8_t {
  HORIZONTAL_DIRECTION_LEFT = 0x09,
  HORIZONTAL_DIRECTION_MIDDLE_LEFT = 0x0A,
  HORIZONTAL_DIRECTION_MIDDLE = 0x06,
  HORIZONTAL_DIRECTION_MIDDLE_RIGHT = 0x0B,
  HORIZONTAL_DIRECTION_RIGHT = 0x0C,
};

// Fixed vertical vane positions (low nibble of byte 16)
enum VerticalDirection : uint8_t {
  VERTICAL_DIRECTION_UP = 0x01,
  VERTICAL_DIRECTION_MIDDLE_UP = 0x02,
  VERTICAL_DIRECTION_MIDDLE = 0x03,
  VERTICAL_DIRECTION_MIDDLE_DOWN = 0x04,
  VERTICAL_DIRECTION_DOWN = 0x05,
};

// Fan modes offered by every set_fan_mode option.
static constexpr climate::ClimateFanModeMask PANASONIC_BASE_FAN_MODES{
    climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH};

class PanasonicClimate : public climate_ir::ClimateIR {
 public:
  PanasonicClimate()
      : climate_ir::ClimateIR(PANASONIC_TEMP_MIN, PANASONIC_TEMP_MAX, 1.0f, false, false, PANASONIC_BASE_FAN_MODES,
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL}) {}

  void set_supports_dry(bool supports_dry) { this->set_mode_supported_(climate::CLIMATE_MODE_DRY, supports_dry); }
  void set_supports_fan_only(bool supports_fan_only) {
    this->set_mode_supported_(climate::CLIMATE_MODE_FAN_ONLY, supports_fan_only);
  }

  void set_fan_mode(SetFanMode fan_mode) {
    this->fan_mode_ = fan_mode;
    this->fan_modes_ = PANASONIC_BASE_FAN_MODES;
    if (fan_mode == PANASONIC_FAN_5L) {
      this->fan_modes_.insert(climate::CLIMATE_FAN_QUIET);
      this->fan_modes_.insert(climate::CLIMATE_FAN_MIDDLE);
    }
  }

  void set_horizontal_default(HorizontalDirection horizontal_direction) {
    this->default_horizontal_direction_ = horizontal_direction;
  }
  void set_vertical_default(VerticalDirection vertical_direction) {
    this->default_vertical_direction_ = vertical_direction;
  }

  void dump_config() override;

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  uint8_t encode_fan_speed_() const;
  optional<climate::ClimateFanMode> decode_fan_speed_(uint8_t speed) const;

  SetFanMode fan_mode_{PANASONIC_FAN_3L};
  HorizontalDirection default_horizontal_direction_{HORIZONTAL_DIRECTION_MIDDLE};
  VerticalDirection default_vertical_direction_{VERTICAL_DIRECTION_DOWN};
};

}  // namespace esphome::panasonic
