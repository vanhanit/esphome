import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.types import ConfigType

try:
    from esphome.components.const import CONF_HORIZONTAL_DEFAULT, CONF_VERTICAL_DEFAULT
except ImportError:  # ESPHome 2026.10 and older, when used as an external component
    CONF_HORIZONTAL_DEFAULT = "horizontal_default"
    CONF_VERTICAL_DEFAULT = "vertical_default"

CODEOWNERS = ["@vanhanit"]
AUTO_LOAD = ["climate_ir"]

panasonic_ns = cg.esphome_ns.namespace("panasonic")
PanasonicClimate = panasonic_ns.class_("PanasonicClimate", climate_ir.ClimateIR)

CONF_SET_FAN_MODE = "set_fan_mode"
SetFanMode = panasonic_ns.enum("SetFanMode")
SETFANMODE = {
    "3levels": SetFanMode.PANASONIC_FAN_3L,
    "5levels": SetFanMode.PANASONIC_FAN_5L,
}

CONF_SUPPORTS_DRY = "supports_dry"
CONF_SUPPORTS_FAN_ONLY = "supports_fan_only"

HorizontalDirection = panasonic_ns.enum("HorizontalDirection")
HORIZONTAL_DIRECTIONS = {
    "left": HorizontalDirection.HORIZONTAL_DIRECTION_LEFT,
    "middle-left": HorizontalDirection.HORIZONTAL_DIRECTION_MIDDLE_LEFT,
    "middle": HorizontalDirection.HORIZONTAL_DIRECTION_MIDDLE,
    "middle-right": HorizontalDirection.HORIZONTAL_DIRECTION_MIDDLE_RIGHT,
    "right": HorizontalDirection.HORIZONTAL_DIRECTION_RIGHT,
}

VerticalDirection = panasonic_ns.enum("VerticalDirection")
VERTICAL_DIRECTIONS = {
    "up": VerticalDirection.VERTICAL_DIRECTION_UP,
    "middle-up": VerticalDirection.VERTICAL_DIRECTION_MIDDLE_UP,
    "middle": VerticalDirection.VERTICAL_DIRECTION_MIDDLE,
    "middle-down": VerticalDirection.VERTICAL_DIRECTION_MIDDLE_DOWN,
    "down": VerticalDirection.VERTICAL_DIRECTION_DOWN,
}


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(PanasonicClimate).extend(
    {
        cv.Optional(CONF_SET_FAN_MODE, default="3levels"): cv.enum(SETFANMODE),
        cv.Optional(CONF_SUPPORTS_DRY, default=False): cv.boolean,
        cv.Optional(CONF_SUPPORTS_FAN_ONLY, default=False): cv.boolean,
        cv.Optional(CONF_HORIZONTAL_DEFAULT, default="middle"): cv.enum(
            HORIZONTAL_DIRECTIONS
        ),
        cv.Optional(CONF_VERTICAL_DEFAULT, default="down"): cv.enum(
            VERTICAL_DIRECTIONS
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)

    cg.add(var.set_fan_mode(config[CONF_SET_FAN_MODE]))
    cg.add(var.set_supports_dry(config[CONF_SUPPORTS_DRY]))
    cg.add(var.set_supports_fan_only(config[CONF_SUPPORTS_FAN_ONLY]))
    cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))
    cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))
