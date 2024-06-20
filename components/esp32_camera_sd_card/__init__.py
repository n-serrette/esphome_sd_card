import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_DATA
from esphome.core import CORE

CONF_ESP32_CAMERA_SD_CARD_ID = "esp32_camera_sd_card"

esp32_camera_sd_card_component_ns = cg.esphome_ns.namespace("esp32_camera_sd_card")
Esp32CameraSDCardComponent = esp32_camera_sd_card_component_ns.class_("Esp32CameraSDCardComponent", cg.Component)

def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Esp32CameraSDCardComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("FS", None)
            cg.add_library("SD_MMC", None)

