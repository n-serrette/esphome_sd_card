import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import (
    StorageBase,
    CONF_STORAGE_ID,
)

DEPENDENCIES = ["storage_base"]

CONF_STORAGE_TYPE = "storage_type"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_STORAGE_ID): cv.use_id(StorageBase),
    cv.Optional(CONF_STORAGE_TYPE): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

async def to_code(config):
    storage_component = await cg.get_variable(config[CONF_STORAGE_ID])

    if CONF_STORAGE_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STORAGE_TYPE])
        cg.add(storage_component.set_storage_type_text_sensor(sens))