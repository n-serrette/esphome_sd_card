import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BYTES,
    ICON_MEMORY,
)
from . import (
    Esp32CameraSDCardComponent,
    CONF_ESP32_CAMERA_SD_CARD_ID,
    CONF_PATH,
)

DEPENDENCIES = ["esp32_camera_sd_card"]

CONF_USED_SPACE = "used_space"
CONF_TOTAL_SPACE = "total_space"
CONF_FILE_SIZE = "file_size"

TYPES = [CONF_USED_SPACE, CONF_TOTAL_SPACE, CONF_USED_SPACE]
SIMPLE_TYPES = [CONF_USED_SPACE, CONF_TOTAL_SPACE]

BASE_CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_BYTES,
    icon=ICON_MEMORY,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_ESP32_CAMERA_SD_CARD_ID): cv.use_id(Esp32CameraSDCardComponent),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TOTAL_SPACE : BASE_CONFIG_SCHEMA,
        CONF_USED_SPACE : BASE_CONFIG_SCHEMA,
        CONF_FILE_SIZE: BASE_CONFIG_SCHEMA.extend(
            {
                cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
            }
        )
    },
    lower=True,
)


async def to_code(config):
    sd_card_component = await cg.get_variable(config[CONF_ESP32_CAMERA_SD_CARD_ID])
    var = await sensor.new_sensor(config)
    if config[CONF_TYPE] in SIMPLE_TYPES:
        func = getattr(sd_card_component, f"set_{config[CONF_TYPE]}_sensor")
        cg.add(func(var))
    elif config[CONF_TYPE] == CONF_FILE_SIZE:
        cg.add(sd_card_component.add_file_size_sensor(var, config[CONF_PATH]))
