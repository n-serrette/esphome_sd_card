import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import SdMmc, CONF_SD_MMC_CARD_ID

DEPENDENCIES = ["sd_mmc_card"]

CONF_SD_CARD_TYPE = "sd_card_type"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SD_MMC_CARD_ID): cv.use_id(SdMmc),
    cv.Optional(CONF_SD_CARD_TYPE): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

async def to_code(config):
    sd_mmc_component = await cg.get_variable(config[CONF_SD_MMC_CARD_ID])

    if CONF_SD_CARD_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SD_CARD_TYPE])
        cg.add(sd_mmc_component.set_sd_card_type_text_sensor(sens))