#include <gtest/gtest.h>
#include <vector>

#include "esphome/components/panasonic/panasonic.h"

namespace esphome::panasonic::testing {

// Records the last transmission instead of sending it
class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  int sends{0};
  remote_base::RawTimings last;

 protected:
  void send_internal(uint32_t, uint32_t) override {
    this->sends++;
    this->last = this->temp_.get_data();
  }
};

class TestPanasonic : public PanasonicClimate {
 public:
  using PanasonicClimate::on_receive;
  using PanasonicClimate::transmit_state;
};

using Frame = std::vector<uint8_t>;

constexpr int32_t ONE_SPACE_THRESHOLD = 900;

// The bytes of both frames of a transmission, skipping the headers and the gap between them
Frame bytes_of(const remote_base::RawTimings &timings) {
  Frame bytes;
  size_t index = 2;  // Skip header 1
  for (size_t frame_size : {8, 19}) {
    for (size_t i = 0; i < frame_size; i++) {
      uint8_t byte = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (-timings[index + 1] > ONE_SPACE_THRESHOLD)
          byte |= 1 << bit;
        index += 2;
      }
      bytes.push_back(byte);
    }
    index += 4;  // Gap mark and space, then header 2
  }
  return bytes;
}

// The frame the component sent before the refactor, for the given bytes 13, 14, 16 and 17
Frame legacy_frame(uint8_t b13, uint8_t b14, uint8_t b16, uint8_t b17) {
  Frame f{0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, b13,
          b14,  0x80, b16,  b17,  0x00, 0x08, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00};
  uint8_t sum = 0;
  for (size_t i = 8; i < 26; i++)
    sum += f[i];
  f[26] = sum;
  return f;
}

struct Fixture {
  TestPanasonic sut;
  CapturingTransmitter transmitter;

  Fixture() {
    this->sut.set_transmitter(&this->transmitter);
    this->sut.mode = climate::CLIMATE_MODE_COOL;
    this->sut.target_temperature = 22;
    this->sut.fan_mode = climate::CLIMATE_FAN_AUTO;
    this->sut.swing_mode = climate::CLIMATE_SWING_OFF;
  }

  Frame sent() const { return bytes_of(this->transmitter.last); }

  bool receive(const remote_base::RawTimings &timings) {
    return this->sut.on_receive(remote_base::RemoteReceiveData(timings, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
  }
};

TEST(PanasonicClimateTest, TransmissionMatchesTheFramesSentBeforeTheRefactor) {
  struct Case {
    const char *name;
    climate::ClimateMode mode;
    float temperature;
    climate::ClimateFanMode fan;
    climate::ClimateSwingMode swing;
    Frame expected;
  };
  const Case cases[] = {
      {"cool 22 auto", climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF,
       legacy_frame(0x39, 0x2C, 0xA5, 0x06)},
      {"heat 30 high", climate::CLIMATE_MODE_HEAT, 30, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF,
       legacy_frame(0x49, 0x3C, 0x75, 0x06)},
      {"auto 16 low", climate::CLIMATE_MODE_HEAT_COOL, 16, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_OFF,
       legacy_frame(0x09, 0x20, 0x45, 0x06)},
      {"dry sends 24", climate::CLIMATE_MODE_DRY, 18, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF,
       legacy_frame(0x29, 0x30, 0x55, 0x06)},
      {"fan only", climate::CLIMATE_MODE_FAN_ONLY, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF,
       legacy_frame(0x69, 0x2C, 0xA5, 0x06)},
      {"off", climate::CLIMATE_MODE_OFF, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF,
       legacy_frame(0x48, 0x2C, 0xA5, 0x06)},
      {"swing vertical", climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_VERTICAL,
       legacy_frame(0x39, 0x2C, 0xAF, 0x06)},
      {"swing horizontal", climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_HORIZONTAL,
       legacy_frame(0x39, 0x2C, 0xA5, 0x0D)},
      {"swing both", climate::CLIMATE_MODE_COOL, 22, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_BOTH,
       legacy_frame(0x39, 0x2C, 0xAF, 0x0D)},
  };
  for (const auto &c : cases) {
    SCOPED_TRACE(c.name);
    Fixture f;
    f.sut.mode = c.mode;
    f.sut.target_temperature = c.temperature;
    f.sut.fan_mode = c.fan;
    f.sut.swing_mode = c.swing;
    f.sut.transmit_state();
    EXPECT_EQ(f.transmitter.sends, 1);
    EXPECT_EQ(f.sent(), c.expected);
  }
}

TEST(PanasonicClimateTest, TargetTemperatureIsRoundedAndClamped) {
  struct Case {
    float target;
    uint8_t sent;
  };
  for (const Case c : {Case{21.4f, 21}, Case{21.6f, 22}, Case{10.0f, 16}, Case{35.0f, 30}}) {
    SCOPED_TRACE(c.target);
    Fixture f;
    f.sut.target_temperature = c.target;
    f.sut.transmit_state();
    EXPECT_EQ(f.sent()[14] >> 1, c.sent);
  }
}

TEST(PanasonicClimateTest, FixedVanesUseTheConfiguredDefaults) {
  Fixture f;
  f.sut.set_vertical_default(VERTICAL_DIRECTION_UP);
  f.sut.set_horizontal_default(HORIZONTAL_DIRECTION_RIGHT);
  f.sut.transmit_state();
  EXPECT_EQ(f.sent()[16] & 0x0F, VERTICAL_DIRECTION_UP);
  EXPECT_EQ(f.sent()[17] & 0x0F, HORIZONTAL_DIRECTION_RIGHT);
}

TEST(PanasonicClimateTest, FiveLevelsUseEverySpeed) {
  struct Case {
    climate::ClimateFanMode fan;
    uint8_t speed;
  };
  const Case cases[] = {{climate::CLIMATE_FAN_QUIET, 3},  {climate::CLIMATE_FAN_LOW, 4},
                        {climate::CLIMATE_FAN_MIDDLE, 5}, {climate::CLIMATE_FAN_MEDIUM, 6},
                        {climate::CLIMATE_FAN_HIGH, 7},   {climate::CLIMATE_FAN_AUTO, 0x0A}};
  for (const auto &c : cases) {
    SCOPED_TRACE(static_cast<int>(c.fan));
    Fixture f;
    f.sut.set_fan_mode(PANASONIC_FAN_5L);
    f.sut.fan_mode = c.fan;
    f.sut.transmit_state();
    EXPECT_EQ(f.sent()[16] >> 4, c.speed);
  }
}

TEST(PanasonicClimateTest, ReceivedTransmissionRestoresTheState) {
  struct Case {
    climate::ClimateMode mode;
    climate::ClimateFanMode fan;
    climate::ClimateSwingMode swing;
  };
  const Case cases[] = {
      {climate::CLIMATE_MODE_HEAT, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_BOTH},
      {climate::CLIMATE_MODE_COOL, climate::CLIMATE_FAN_LOW, climate::CLIMATE_SWING_VERTICAL},
      {climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_HORIZONTAL},
      {climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF},
      {climate::CLIMATE_MODE_OFF, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_SWING_OFF},
  };
  for (const auto &c : cases) {
    SCOPED_TRACE(static_cast<int>(c.mode));
    Fixture sender;
    sender.sut.mode = c.mode;
    sender.sut.target_temperature = 27;
    sender.sut.fan_mode = c.fan;
    sender.sut.swing_mode = c.swing;
    sender.sut.transmit_state();

    Fixture receiver;
    receiver.sut.mode = climate::CLIMATE_MODE_DRY;
    ASSERT_TRUE(receiver.receive(sender.transmitter.last));
    EXPECT_EQ(receiver.sut.mode, c.mode);
    EXPECT_FLOAT_EQ(receiver.sut.target_temperature, 27);
    EXPECT_EQ(receiver.sut.fan_mode, c.fan);
    EXPECT_EQ(receiver.sut.swing_mode, c.swing);
  }
}

TEST(PanasonicClimateTest, ReceiveRejectsABadChecksum) {
  Fixture f;
  f.sut.transmit_state();
  auto timings = f.transmitter.last;
  // Flip the first bit of byte 13, which sits in the second frame after header 2 and the gap
  const size_t bit_index = 2 + 8 * 16 + 2 + 2 + 5 * 16;
  timings[bit_index + 1] = timings[bit_index + 1] < -ONE_SPACE_THRESHOLD ? -430 : -1350;
  EXPECT_FALSE(f.receive(timings));
}

TEST(PanasonicClimateTest, ReceiveRejectsOtherProtocols) {
  Fixture f;
  remote_base::RawTimings timings{9000, -4500, 560, -560, 560};
  EXPECT_FALSE(f.receive(timings));
}

TEST(PanasonicClimateTest, FanModesFor3Levels) {
  PanasonicClimate climate;
  climate.set_fan_mode(PANASONIC_FAN_3L);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_AUTO));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_LOW));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_MEDIUM));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_HIGH));
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_MIDDLE));
  EXPECT_FALSE(traits.supports_fan_mode(climate::CLIMATE_FAN_QUIET));
}

TEST(PanasonicClimateTest, FanModesFor5Levels) {
  PanasonicClimate climate;
  climate.set_fan_mode(PANASONIC_FAN_5L);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_MIDDLE));
  EXPECT_TRUE(traits.supports_fan_mode(climate::CLIMATE_FAN_QUIET));
}

TEST(PanasonicClimateTest, DryAndFanOnlyAreOptIn) {
  PanasonicClimate climate;
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_DRY));
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_FAN_ONLY));
  climate.set_supports_dry(true);
  climate.set_supports_fan_only(true);
  EXPECT_TRUE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_DRY));
  EXPECT_TRUE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_FAN_ONLY));
}

}  // namespace esphome::panasonic::testing
