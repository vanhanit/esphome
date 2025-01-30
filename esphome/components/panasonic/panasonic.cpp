#include "panasonic.h"
#include "esphome/core/log.h"

namespace esphome {
namespace panasonic {

static const char *const TAG = "panasonic.climate";

// Pulse parameters in usec
const uint16_t PANASONIC_BIT_MARK = 400;
const uint16_t PANASONIC_END_MARK = 550;
const uint16_t PANASONIC_ONE_SPACE = 1350;
const uint16_t PANASONIC_ZERO_SPACE = 430;
const uint16_t PANASONIC_HEADER_MARK = 3500;
const uint16_t PANASONIC_HEADER_SPACE = 1700;
const uint16_t PANASONIC_MIN_GAP = 10000;

// Marker bytes
const uint8_t PANASONIC_BYTE00 = 0X02;
const uint8_t PANASONIC_BYTE01 = 0X20;
const uint8_t PANASONIC_BYTE02 = 0Xe0;
const uint8_t PANASONIC_BYTE03 = 0X04;
const uint8_t PANASONIC_BYTE07 = 0X06;
const uint8_t PANASONIC_BYTE08 = 0X02;
const uint8_t PANASONIC_BYTE09 = 0X20;
const uint8_t PANASONIC_BYTE10 = 0Xe0;
const uint8_t PANASONIC_BYTE11 = 0X04;
const uint8_t PANASONIC_BYTE15 = 0X80;
const uint8_t PANASONIC_BYTE19 = 0X08;
const uint8_t PANASONIC_BYTE20 = 0X80;
const uint8_t PANASONIC_BYTE23 = 0X80;

climate::ClimateTraits PanasonicClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_action(false);
  traits.set_visual_min_temperature(PANASONIC_TEMP_MIN);
  traits.set_visual_max_temperature(PANASONIC_TEMP_MAX);
  traits.set_visual_temperature_step(1.0f);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF});

  if (this->supports_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  if (this->supports_heat_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);

  if (this->supports_cool_ && this->supports_heat_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT_COOL);

  if (this->supports_dry_)
    traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  if (this->supports_fan_only_)
    traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);

  // Default to only 3 levels in ESPHome even if most unit supports 4. The 3rd level is not used.
  traits.set_supported_fan_modes(
      {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
  if (this->fan_mode_ == PANASONIC_FAN_LOWEST)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_QUIET);
  if (this->fan_mode_ == PANASONIC_FAN_MEDIUM)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_MIDDLE);  // Shouldn't be used for this but it helps

  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                    climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});

  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE /*, climate::CLIMATE_PRESET_ECO*/});

  return traits;
}

void PanasonicClimate::transmit_state() {
  // ------------------------------------------------------------------------------
  // | Byte  | Description                                                        |
  // ------------------------------------------------------------------------------
  // | 0     | Constant: 0x02                                                     |
  // | 1     | Constant: 0x20                                                     |
  // | 2     | Constant: 0xe0                                                     |
  // | 3     | Constant: 0x04                                                     |
  // | 4-6   | Constant: 0x00                                                     |
  // | 7     | Constant: 0x06                                                     |
  // | 8     | Constant: 0x02                                                     |
  // | 9     | Constant: 0x20                                                     |
  // | 10    | Constant: 0xe0                                                     |
  // | 11    | Constant: 0x04                                                     |
  // | 13    | reserved:1 | mode:3 | reserved:1 | timer_off:1 | timer_on:1 | on:1 |
  // | 14    | reserved:2 | normal_mode:1 | temperature:4 | reserved:1            |
  // | 15    | Constant: 0x80                                                     |
  // | 16    | fan_speed:4 | swing_vertical:4                                     |
  // | 17    | reserved:4 | swing_horizontal:4                                    |
  // | 18-20 | reserved:1 | timer_minute_off:11 | reserved:1 | timer_minute_on:11 |
  // | 21-22 | reserved: 0x00                                                     |
  // | 23    | Constant: 0x80                                                     |
  // | 24-25 | reserved:5 | time:11                                               |
  // | 26    | Checksum: SUM[8..25]                                               |
  // ------------------------------------------------------------------------------

  uint8_t remote_state[27] = {PANASONIC_BYTE00,
                              PANASONIC_BYTE01,
                              PANASONIC_BYTE02,
                              PANASONIC_BYTE03,
                              0x00,
                              0x00,
                              0x00,
                              PANASONIC_BYTE07,
                              PANASONIC_BYTE08,
                              PANASONIC_BYTE09,
                              PANASONIC_BYTE10,
                              PANASONIC_BYTE11,
                              0x00,
                              0x00,
                              0x00,
                              PANASONIC_BYTE15,
                              0x00,
                              0x00,
                              0x00,
                              PANASONIC_BYTE19,
                              PANASONIC_BYTE20,
                              0x00,
                              0x00,
                              PANASONIC_BYTE23,
                              0x00,
                              0x00,
                              0x00};

  remote_state[13] = 0x01;  // Default on
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      remote_state[13] |= Mode::HEAT << 4;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[13] |= Mode::DRY << 4;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[13] |= Mode::COOL << 4;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[13] |= Mode::AUTO << 4;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[13] |= Mode::FAN << 4;
      break;
    case climate::CLIMATE_MODE_OFF:
      remote_state[13] = 0x00;  // Off
    default:
      if (this->supports_heat_) {
        remote_state[13] |= Mode::HEAT << 4;
      } else {
        remote_state[13] |= Mode::COOL << 4;
      }
      break;
  }
  remote_state[13] |= 1 << 3;

  // Temperature
  if (this->mode == climate::CLIMATE_MODE_DRY) {
    remote_state[14] = ((24 - PANASONIC_TEMP_MIN) & 0x0f) << 1;
  } else {
    remote_state[14] =
        ((uint8_t) roundf(clamp<float>(this->target_temperature, PANASONIC_TEMP_MIN, PANASONIC_TEMP_MAX) -
                          PANASONIC_TEMP_MIN))
        << 1;
  }
  remote_state[14] |= 1 << 5;  // Normal mode

  // Swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_HORIZONTAL:
      remote_state[16] = VerticalDirection::VERTICAL_DIRECTION_DOWN;
      remote_state[17] = HorizontalDirection::HORIZONTAL_DIRECTION_AUTO;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      remote_state[16] = VerticalDirection::VERTICAL_DIRECTION_AUTO;
      remote_state[17] = HorizontalDirection::HORIZONTAL_DIRECTION_MIDDLE;
      break;
    case climate::CLIMATE_SWING_BOTH:
      remote_state[16] = VerticalDirection::VERTICAL_DIRECTION_AUTO;
      remote_state[17] = HorizontalDirection::HORIZONTAL_DIRECTION_AUTO;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      remote_state[16] = VerticalDirection::VERTICAL_DIRECTION_DOWN;
      remote_state[17] = HorizontalDirection::HORIZONTAL_DIRECTION_MIDDLE;
      break;
  }

  ESP_LOGD(TAG, "default_horizontal_direction_: %02X", this->default_horizontal_direction_);
  ESP_LOGD(TAG, "default_vertical_direction_: %02X", this->default_vertical_direction_);

  // Fan Speed
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_QUIET:
      remote_state[16] |= SetFanMode::PANASONIC_FAN_LOWEST << 4;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state[16] |= SetFanMode::PANASONIC_FAN_LOW << 4;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[16] |= SetFanMode::PANASONIC_FAN_MEDIUM << 4;
      break;
    case climate::CLIMATE_FAN_MIDDLE:
      remote_state[16] |= SetFanMode::PANASONIC_FAN_HIGH << 4;
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[16] |= SetFanMode::PANASONIC_FAN_HIGHEST << 4;
      break;
    default:
      remote_state[16] |= SetFanMode::PANASONIC_FAN_AUTO << 4;
      break;
  }

  ESP_LOGD(TAG, "fan: %02x state: %01x", this->fan_mode.value(), remote_state[16]);

  // Special modes
  switch (this->preset.value()) {
    /*
    case climate::CLIMATE_PRESET_ECO:
      remote_state[6] = PANASONIC_MODE_COOL | PANASONIC_OTHERWISE;
      remote_state[8] = (remote_state[8] & ~7) | PANASONIC_MODE_A_COOL;
      remote_state[14] = PANASONIC_ECONOCOOL;
      break;
      */
    case climate::CLIMATE_PRESET_NONE:
    default:
      break;
  }

  // Checksum
  remote_state[26] = 0;
  for (int i = 8; i < 26; i++) {
    remote_state[26] += remote_state[i];
  }

  ESP_LOGD(TAG, "sending: %s", format_hex_pretty(remote_state, 27).c_str());

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header 1
  data->mark(PANASONIC_HEADER_MARK);
  data->space(PANASONIC_HEADER_SPACE);

  uint8_t p;
  for (p = 0; p < 8; p++) {
    uint8_t payload = remote_state[p];
    for (uint8_t bitNo = 0; bitNo < 8; bitNo++) {
      data->mark(PANASONIC_BIT_MARK);
      bool bit = payload & 0x01;
      data->space(bit ? PANASONIC_ONE_SPACE : PANASONIC_ZERO_SPACE);
      payload >>= 1;
    }
  }
  data->mark(PANASONIC_BIT_MARK);
  data->space(PANASONIC_MIN_GAP);

  // Header 2
  data->mark(PANASONIC_HEADER_MARK);
  data->space(PANASONIC_HEADER_SPACE);

  for (; p < sizeof(remote_state); p++) {
    uint8_t payload = remote_state[p];
    for (uint8_t bitNo = 0; bitNo < 8; bitNo++) {
      data->mark(PANASONIC_BIT_MARK);
      bool bit = payload & 0x01;
      data->space(bit ? PANASONIC_ONE_SPACE : PANASONIC_ZERO_SPACE);
      payload >>= 1;
    }
  }
  data->mark(PANASONIC_END_MARK);

  transmit.perform();
}

bool PanasonicClimate::parse_state_frame_(const uint8_t frame[]) { return false; }

bool PanasonicClimate::on_receive(remote_base::RemoteReceiveData data) {
  return false;
  /*
  uint8_t state_frame[27] = {};

  if (!data.expect_item(PANASONIC_HEADER_MARK, PANASONIC_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  for (uint8_t pos = 0; pos < 27; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(PANASONIC_BIT_MARK, PANASONIC_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(PANASONIC_BIT_MARK, PANASONIC_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", pos, bit);
        return false;
      }
    }
    state_frame[pos] = byte;

    // Check Header && Footer
    if ((pos == 0 && byte != PANASONIC_BYTE00) || (pos == 1 && byte != PANASONIC_BYTE01) ||
        (pos == 2 && byte != PANASONIC_BYTE02) || (pos == 3 && byte != PANASONIC_BYTE03) ||
        (pos == 7 && byte != PANASONIC_BYTE07) || (pos == 8 && byte != PANASONIC_BYTE08) ||
        (pos == 9 && byte != PANASONIC_BYTE09) || (pos == 10 && byte != PANASONIC_BYTE10) ||
        (pos == 11 && byte != PANASONIC_BYTE11) || (pos == 15 && byte != PANASONIC_BYTE15)) {
      ESP_LOGV(TAG, "Byte %d fail - invalid value", pos);
      return false;
    }
  }

  // On/Off and Mode
  if (state_frame[5] == PANASONIC_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (state_frame[6]) {
      case PANASONIC_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case PANASONIC_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case PANASONIC_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case PANASONIC_MODE_FAN_ONLY:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case PANASONIC_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }

  // Temp
  this->target_temperature = state_frame[7] + PANASONIC_TEMP_MIN;

  // Fan
  uint8_t fan = state_frame[9] & 0x07;  //(Bit 0,1,2 = Speed)
  // Map of Climate fan mode to this device expected value
  // For 3Level: Low = 1, Medium = 2, High = 3
  // For 4Level: Low = 1, Middle = 2, Medium = 3, High = 4
  // For 5Level: Low = 1, Middle = 2, Medium = 3, High = 4
  // For 4Level + Quiet: Low = 1, Middle = 2, Medium = 3, High = 4, Quiet = 5
  climate::ClimateFanMode modes_mapping[8] = {
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      this->fan_mode_ == PANASONIC_FAN_3L ? climate::CLIMATE_FAN_MEDIUM : climate::CLIMATE_FAN_MIDDLE,
      this->fan_mode_ == PANASONIC_FAN_3L ? climate::CLIMATE_FAN_HIGH : climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
      climate::CLIMATE_FAN_QUIET,
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_AUTO};
  this->fan_mode = modes_mapping[fan];

  // Wide Vane
  uint8_t wide_vane = state_frame[8] & 0xF0;  // Bits 4,5,6,7
  switch (wide_vane) {
    case PANASONIC_WIDE_VANE_SWING:
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
      break;
    default:
      this->swing_mode = climate::CLIMATE_SWING_OFF;
      break;
  }

  // Vertical Vane
  uint8_t vertical_vane = state_frame[9] & 0x38;  // Bits 3,4,5
  switch (vertical_vane) {
    case PANASONIC_VERTICAL_VANE_SWING:
      if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) {
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      }
      break;
  }

  switch (state_frame[14]) {
    case PANASONIC_ECONOCOOL:
      this->preset = climate::CLIMATE_PRESET_ECO;
      break;
    case PANASONIC_NIGHTMODE:
      this->preset = climate::CLIMATE_PRESET_SLEEP;
      break;
  }

  ESP_LOGV(TAG, "Receiving: %s", format_hex_pretty(state_frame, 18).c_str());

  this->publish_state();
  return true;
  */
}

}  // namespace panasonic
}  // namespace esphome
