import esphome.codegen as cg
from esphome.components import climate_ir
from esphome.components.const import CONF_HORIZONTAL_DEFAULT, CONF_VERTICAL_DEFAULT
import esphome.config_validation as cv
from esphome.types import ConfigType

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
HORIZONTAL_DIRECTIONS = HorizontalDirection.options(
    "left",
    "middle-left",
    "middle",
    "middle-right",
    "right",
    prefix="HORIZONTAL_DIRECTION_",
)

VerticalDirection = panasonic_ns.enum("VerticalDirection")
VERTICAL_DIRECTIONS = VerticalDirection.options(
    "up",
    "middle-up",
    "middle",
    "middle-down",
    "down",
    prefix="VERTICAL_DIRECTION_",
)


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
