#include "panasonic.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstring>

namespace esphome::panasonic {

ESPHOME_LOG_TAG(TAG, "panasonic.climate");

// A command is two frames: an 8 byte preamble frame, a gap, then the 19 byte state frame.
static constexpr size_t PANASONIC_FRAME1_SIZE = 8;
static constexpr size_t PANASONIC_FRAME2_SIZE = 19;
static constexpr size_t PANASONIC_STATE_SIZE = PANASONIC_FRAME1_SIZE + PANASONIC_FRAME2_SIZE;

// Pulse parameters in usec
static constexpr uint16_t PANASONIC_HEADER_MARK = 3500;
static constexpr uint16_t PANASONIC_HEADER_SPACE = 1700;
static constexpr uint16_t PANASONIC_BIT_MARK = 400;
static constexpr uint16_t PANASONIC_ONE_SPACE = 1350;
static constexpr uint16_t PANASONIC_ZERO_SPACE = 430;
static constexpr uint16_t PANASONIC_END_MARK = 550;
static constexpr uint16_t PANASONIC_MIN_GAP = 10000;

// Byte 13: power and mode
static constexpr uint8_t PANASONIC_POWER_ON = 0x01;
static constexpr uint8_t PANASONIC_BYTE13_CONSTANT = 0x08;
static constexpr uint8_t PANASONIC_MODE_SHIFT = 4;
static constexpr uint8_t PANASONIC_MODE_MASK = 0x07;
static constexpr uint8_t PANASONIC_MODE_AUTO = 0x00;
static constexpr uint8_t PANASONIC_MODE_DRY = 0x02;
static constexpr uint8_t PANASONIC_MODE_COOL = 0x03;
static constexpr uint8_t PANASONIC_MODE_HEAT = 0x04;
static constexpr uint8_t PANASONIC_MODE_FAN_ONLY = 0x06;

// Byte 14: target temperature in whole degrees Celsius, bits 1-5
static constexpr uint8_t PANASONIC_TEMP_SHIFT = 1;
static constexpr uint8_t PANASONIC_TEMP_MASK = 0x1F;
// The unit is sent this temperature in dry mode
static constexpr uint8_t PANASONIC_DRY_TEMP = 24;

// Byte 16: fan speed (high nibble) and vertical vane (low nibble)
static constexpr uint8_t PANASONIC_FAN_AUTO = 0x0A;
static constexpr uint8_t PANASONIC_FAN_SPEED_1 = 0x03;  // Lowest
static constexpr uint8_t PANASONIC_FAN_SPEED_2 = 0x04;
static constexpr uint8_t PANASONIC_FAN_SPEED_3 = 0x05;
static constexpr uint8_t PANASONIC_FAN_SPEED_4 = 0x06;
static constexpr uint8_t PANASONIC_FAN_SPEED_5 = 0x07;  // Highest
static constexpr uint8_t PANASONIC_VERTICAL_SWING = 0x0F;

// Byte 17: horizontal vane (low nibble)
static constexpr uint8_t PANASONIC_HORIZONTAL_SWING = 0x0D;

// Byte 26: checksum over the second frame
static constexpr size_t PANASONIC_CHECKSUM_START = PANASONIC_FRAME1_SIZE;
static constexpr size_t PANASONIC_CHECKSUM_INDEX = PANASONIC_STATE_SIZE - 1;

// ------------------------------------------------------------------------------
// | Byte  | Description                                                        |
// ------------------------------------------------------------------------------
// | 0-7   | Constant frame 1: 0x02 0x20 0xE0 0x04 0x00 0x00 0x00 0x06          |
// | 8-11  | Constant: 0x02 0x20 0xE0 0x04                                      |
// | 12    | Constant: 0x00                                                     |
// | 13    | reserved:1 | mode:3 | constant 1:1 | timer_off:1 | timer_on:1 | on:1|
// | 14    | reserved:2 | temperature:5 | half_degree:1                         |
// | 15    | Constant: 0x80                                                     |
// | 16    | fan_speed:4 | vertical_vane:4                                       |
// | 17    | reserved:4 | horizontal_vane:4                                     |
// | 18-20 | Timers (unused), model specific constants in 19 and 20             |
// | 21-22 | reserved: 0x00                                                     |
// | 23    | Model specific constant                                            |
// | 24-25 | Clock (unused)                                                     |
// | 26    | Checksum: SUM[8..25]                                               |
// ------------------------------------------------------------------------------
static constexpr uint8_t PANASONIC_TEMPLATE[PANASONIC_STATE_SIZE] = {
    0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06,  // Frame 1
    0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
};
// Bytes of the second frame that every Panasonic AC remote sends unchanged
static constexpr size_t PANASONIC_FIXED_BYTES = 12;

static uint8_t checksum(const uint8_t *state) {
  uint8_t sum = 0;
  for (size_t i = PANASONIC_CHECKSUM_START; i < PANASONIC_CHECKSUM_INDEX; i++)
    sum += state[i];
  return sum;
}

// Bits are sent least significant bit first
static void encode_bytes(remote_base::RemoteTransmitData *data, const uint8_t *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    for (uint8_t bit = 0; bit < 8; bit++)
      data->item(PANASONIC_BIT_MARK, (bytes[i] >> bit) & 1 ? PANASONIC_ONE_SPACE : PANASONIC_ZERO_SPACE);
  }
}

static bool decode_bytes(remote_base::RemoteReceiveData &data, uint8_t *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t byte = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(PANASONIC_BIT_MARK, PANASONIC_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(PANASONIC_BIT_MARK, PANASONIC_ZERO_SPACE)) {
        return false;
      }
    }
    bytes[i] = byte;
  }
  return true;
}

static uint8_t encode_mode(climate::ClimateMode mode, bool supports_heat) {
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT:
      return PANASONIC_MODE_HEAT;
    case climate::CLIMATE_MODE_DRY:
      return PANASONIC_MODE_DRY;
    case climate::CLIMATE_MODE_COOL:
      return PANASONIC_MODE_COOL;
    case climate::CLIMATE_MODE_HEAT_COOL:
      return PANASONIC_MODE_AUTO;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return PANASONIC_MODE_FAN_ONLY;
    case climate::CLIMATE_MODE_OFF:
    default:
      // The unit is switched off with the power bit; it still expects a mode
      return supports_heat ? PANASONIC_MODE_HEAT : PANASONIC_MODE_COOL;
  }
}

static optional<climate::ClimateMode> decode_mode(uint8_t mode) {
  switch (mode) {
    case PANASONIC_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case PANASONIC_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case PANASONIC_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case PANASONIC_MODE_AUTO:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case PANASONIC_MODE_FAN_ONLY:
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return {};
  }
}

uint8_t PanasonicClimate::encode_fan_speed_() const {
  const bool five_levels = this->fan_mode_ == PANASONIC_FAN_5L;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_QUIET:
      return PANASONIC_FAN_SPEED_1;
    case climate::CLIMATE_FAN_LOW:
      return PANASONIC_FAN_SPEED_2;
    case climate::CLIMATE_FAN_MIDDLE:
      return PANASONIC_FAN_SPEED_3;
    case climate::CLIMATE_FAN_MEDIUM:
      return five_levels ? PANASONIC_FAN_SPEED_4 : PANASONIC_FAN_SPEED_3;
    case climate::CLIMATE_FAN_HIGH:
      return PANASONIC_FAN_SPEED_5;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return PANASONIC_FAN_AUTO;
  }
}

optional<climate::ClimateFanMode> PanasonicClimate::decode_fan_speed_(uint8_t speed) const {
  const bool five_levels = this->fan_mode_ == PANASONIC_FAN_5L;
  switch (speed) {
    case PANASONIC_FAN_AUTO:
      return climate::CLIMATE_FAN_AUTO;
    case PANASONIC_FAN_SPEED_1:
      return five_levels ? climate::CLIMATE_FAN_QUIET : climate::CLIMATE_FAN_LOW;
    case PANASONIC_FAN_SPEED_2:
      return climate::CLIMATE_FAN_LOW;
    case PANASONIC_FAN_SPEED_3:
      return five_levels ? climate::CLIMATE_FAN_MIDDLE : climate::CLIMATE_FAN_MEDIUM;
    case PANASONIC_FAN_SPEED_4:
      return five_levels ? climate::CLIMATE_FAN_MEDIUM : climate::CLIMATE_FAN_HIGH;
    case PANASONIC_FAN_SPEED_5:
      return climate::CLIMATE_FAN_HIGH;
    default:
      return {};
  }
}

void PanasonicClimate::transmit_state() {
  uint8_t remote_state[PANASONIC_STATE_SIZE];
  memcpy(remote_state, PANASONIC_TEMPLATE, sizeof(remote_state));

  // Power and mode
  const bool supports_heat = this->modes_.count(climate::CLIMATE_MODE_HEAT) != 0;
  remote_state[13] = (encode_mode(this->mode, supports_heat) << PANASONIC_MODE_SHIFT) | PANASONIC_BYTE13_CONSTANT;
  if (this->mode != climate::CLIMATE_MODE_OFF)
    remote_state[13] |= PANASONIC_POWER_ON;

  // Temperature
  uint8_t temperature = PANASONIC_DRY_TEMP;
  if (this->mode != climate::CLIMATE_MODE_DRY && !std::isnan(this->target_temperature)) {
    temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, PANASONIC_TEMP_MIN, PANASONIC_TEMP_MAX));
  }
  remote_state[14] = temperature << PANASONIC_TEMP_SHIFT;

  // Vanes: a swinging axis is sent as auto, a fixed one at its configured position
  const auto swing_mode = this->swing_mode;
  const bool swing_vertical =
      swing_mode == climate::CLIMATE_SWING_VERTICAL || swing_mode == climate::CLIMATE_SWING_BOTH;
  const bool swing_horizontal =
      swing_mode == climate::CLIMATE_SWING_HORIZONTAL || swing_mode == climate::CLIMATE_SWING_BOTH;
  remote_state[16] =
      swing_vertical ? PANASONIC_VERTICAL_SWING : static_cast<uint8_t>(this->default_vertical_direction_);
  remote_state[17] =
      swing_horizontal ? PANASONIC_HORIZONTAL_SWING : static_cast<uint8_t>(this->default_horizontal_direction_);

  // Fan speed
  remote_state[16] |= this->encode_fan_speed_() << 4;

  remote_state[PANASONIC_CHECKSUM_INDEX] = checksum(remote_state);

#ifdef ESPHOME_LOG_HAS_DEBUG
  char hex[format_hex_pretty_size(PANASONIC_STATE_SIZE)];
  ESP_LOGD(TAG, "Sending: %s", format_hex_pretty_to(hex, remote_state, PANASONIC_STATE_SIZE));
#endif

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(38000);
  // Two headers, a mark and a space per bit, the gap and the end mark
  data->reserve(4 + PANASONIC_STATE_SIZE * 16 + 3);

  // Frame 1
  data->item(PANASONIC_HEADER_MARK, PANASONIC_HEADER_SPACE);
  encode_bytes(data, remote_state, PANASONIC_FRAME1_SIZE);
  data->item(PANASONIC_BIT_MARK, PANASONIC_MIN_GAP);

  // Frame 2
  data->item(PANASONIC_HEADER_MARK, PANASONIC_HEADER_SPACE);
  encode_bytes(data, remote_state + PANASONIC_FRAME1_SIZE, PANASONIC_FRAME2_SIZE);
  data->mark(PANASONIC_END_MARK);

  transmit.perform();
}

bool PanasonicClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state[PANASONIC_STATE_SIZE];

  // Frame 1
  if (!data.expect_item(PANASONIC_HEADER_MARK, PANASONIC_HEADER_SPACE) ||
      !decode_bytes(data, state, PANASONIC_FRAME1_SIZE) ||
      memcmp(state, PANASONIC_TEMPLATE, PANASONIC_FRAME1_SIZE) != 0) {
    ESP_LOGV(TAG, "Frame 1 fail");
    return false;
  }
  if (!data.expect_pulse_with_gap(PANASONIC_BIT_MARK, PANASONIC_MIN_GAP)) {
    ESP_LOGV(TAG, "Gap fail");
    return false;
  }

  // Frame 2
  uint8_t *frame2 = state + PANASONIC_FRAME1_SIZE;
  if (!data.expect_item(PANASONIC_HEADER_MARK, PANASONIC_HEADER_SPACE) ||
      !decode_bytes(data, frame2, PANASONIC_FRAME2_SIZE) ||
      memcmp(frame2, PANASONIC_TEMPLATE + PANASONIC_FRAME1_SIZE, PANASONIC_FIXED_BYTES - PANASONIC_FRAME1_SIZE) != 0) {
    ESP_LOGV(TAG, "Frame 2 fail");
    return false;
  }
  const uint8_t expected = checksum(state);
  if (expected != state[PANASONIC_CHECKSUM_INDEX]) {
    ESP_LOGV(TAG, "Checksum fail: expected %02X, got %02X", expected, state[PANASONIC_CHECKSUM_INDEX]);
    return false;
  }

#ifdef ESPHOME_LOG_HAS_VERBOSE
  char hex[format_hex_pretty_size(PANASONIC_STATE_SIZE)];
  ESP_LOGV(TAG, "Receiving: %s", format_hex_pretty_to(hex, state, PANASONIC_STATE_SIZE));
#endif

  // Power and mode; an unknown mode keeps the current one
  if ((state[13] & PANASONIC_POWER_ON) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    auto mode = decode_mode((state[13] >> PANASONIC_MODE_SHIFT) & PANASONIC_MODE_MASK);
    if (mode.has_value())
      this->mode = *mode;
  }

  // Temperature
  const uint8_t temperature = (state[14] >> PANASONIC_TEMP_SHIFT) & PANASONIC_TEMP_MASK;
  if (temperature >= PANASONIC_TEMP_MIN && temperature <= PANASONIC_TEMP_MAX)
    this->target_temperature = temperature;

  // Fan speed; an unknown speed keeps the current one
  auto fan_mode = this->decode_fan_speed_(state[16] >> 4);
  if (fan_mode.has_value())
    this->fan_mode = fan_mode;

  // Vanes
  const bool swing_vertical = (state[16] & 0x0F) == PANASONIC_VERTICAL_SWING;
  const bool swing_horizontal = (state[17] & 0x0F) == PANASONIC_HORIZONTAL_SWING;
  if (swing_vertical && swing_horizontal) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (swing_vertical) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_horizontal) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

void PanasonicClimate::dump_config() {
  climate_ir::ClimateIR::dump_config();
  ESP_LOGCONFIG(TAG,
                "  Fan speeds: %u\n"
                "  Horizontal default: 0x%02X\n"
                "  Vertical default: 0x%02X",
                this->fan_mode_ == PANASONIC_FAN_5L ? 5u : 3u, this->default_horizontal_direction_,
                this->default_vertical_direction_);
}

}  // namespace esphome::panasonic
