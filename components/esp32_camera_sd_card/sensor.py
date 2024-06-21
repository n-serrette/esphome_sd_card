import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BYTES,
    ICON_MEMORY,
)
from . import Esp32CameraSDCardComponent, CONF_ESP32_CAMERA_SD_CARD_ID

DEPENDENCIES = ["esp32_camera_sd_card"]

TYPES = ["used_space", "total_space"]

CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_BYTES,
    icon=ICON_MEMORY,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
        cv.GenerateID(CONF_ESP32_CAMERA_SD_CARD_ID): cv.use_id(Esp32CameraSDCardComponent),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ESP32_CAMERA_SD_CARD_ID])
    var = await sensor.new_sensor(config)
    func = getattr(hub, f"set_{config[CONF_TYPE]}_sensor")
    cg.add(func(var))