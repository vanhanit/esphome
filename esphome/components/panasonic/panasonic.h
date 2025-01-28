#pragma once
#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome {
  namespace panasonic {

  // Temperature
  const uint8_t PANASONIC_TEMP_MIN = 16;  // Celsius
  const uint8_t PANASONIC_TEMP_MAX = 30;  // Celsius

  enum Mode { AUTO = 0b000, HEAT = 0b100, COOL = 0b011, DRY = 0b010, FAN = 0b110 };

  // Fan mode
  enum SetFanMode {
    PANASONIC_FAN_AUTO = 0b1010,
    PANASONIC_FAN_LOWEST = 0b0011,
    PANASONIC_FAN_LOW = 0b0100,
    PANASONIC_FAN_MEDIUM = 0b0101,
    PANASONIC_FAN_HIGH = 0b0110,
    PANASONIC_FAN_HIGHEST = 0b0111
  };

  // Enum to represent horizontal directios
  enum HorizontalDirection {
    HORIZONTAL_DIRECTION_AUTO = 0b1101,
    HORIZONTAL_DIRECTION_MIDDLE = 0b0110,
    HORIZONTAL_DIRECTION_LEFT = 0b1001,
    HORIZONTAL_DIRECTION_MIDDLE_LEFT = 0b1010,
    HORIZONTAL_DIRECTION_MIDDLE_RIGHT = 0b1011,
    HORIZONTAL_DIRECTION_RIGHT = 0b1100
  };

  // Enum to represent vertical directions
  enum VerticalDirection {
    VERTICAL_DIRECTION_AUTO = 0b1010,
    VERTICAL_DIRECTION_UP = 0b0011,
    VERTICAL_DIRECTION_MIDDLE_UP = 0b0100,
    VERTICAL_DIRECTION_MIDDLE = 0b0101,
    VERTICAL_DIRECTION_MIDDLE_DOWN = 0b0110,
    VERTICAL_DIRECTION_DOWN = 0b0111
  };

  class PanasonicClimate : public climate_ir::ClimateIR {
   public:
    PanasonicClimate()
        : climate_ir::ClimateIR(PANASONIC_TEMP_MIN, PANASONIC_TEMP_MAX, 1.0f, true, true,
                                {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MIDDLE,
                                 climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET},
                                {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                 climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL},
                                {climate::CLIMATE_PRESET_NONE /*, climate::CLIMATE_PRESET_ECO */}) {}

    void set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
    void set_supports_dry(bool supports_dry) { this->supports_dry_ = supports_dry; }
    void set_supports_fan_only(bool supports_fan_only) { this->supports_fan_only_ = supports_fan_only; }
    void set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }

    void set_fan_mode(SetFanMode fan_mode) { this->fan_mode_ = fan_mode; }

    void set_horizontal_default(HorizontalDirection horizontal_direction) {
      this->default_horizontal_direction_ = horizontal_direction;
    }
    void set_vertical_default(VerticalDirection vertical_direction) {
      this->default_vertical_direction_ = vertical_direction;
    }

   protected:
    // Transmit via IR the state of this climate controller.
    void transmit_state() override;
    // Handle received IR Buffer
    bool on_receive(remote_base::RemoteReceiveData data) override;
    bool parse_state_frame_(const uint8_t frame[]);

    SetFanMode fan_mode_;

    HorizontalDirection default_horizontal_direction_;
    VerticalDirection default_vertical_direction_;

    climate::ClimateTraits traits() override;
  };

  }  // namespace panasonic
}  // namespace esphome
