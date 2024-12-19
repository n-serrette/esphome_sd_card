import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import (
    CONF_ID
)
from esphome.core import coroutine_with_priority, CORE
from .. import sd_mmc_card

CONF_URL_PREFIX = "url_prefix"
CONF_ROOT_PATH = "root_path"

AUTO_LOAD = ["web_server_base"]
DEPENDENCIES = ["sd_mmc_card"]

sd_file_server_ns = cg.esphome_ns.namespace("sd_file_server")
SDFileServer = sd_file_server_ns.class_("SDFileServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SDFileServer),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.GenerateID(sd_mmc_card.CONF_SD_MMC_CARD_ID): cv.use_id(sd_mmc_card.SdMmc),
            cv.Optional(CONF_URL_PREFIX, default="file"): cv.string_strict,
            cv.Optional(CONF_ROOT_PATH, default="/"): cv.string_strict,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)

@coroutine_with_priority(45.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    
    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
    sdmmc = await cg.get_variable(config[sd_mmc_card.CONF_SD_MMC_CARD_ID])
    cg.add(var.set_sd_mmc_card(sdmmc))
    cg.add(var.set_url_prefix(config[CONF_URL_PREFIX]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))

    cg.add_define("USE_SD_CARD_WEBSERVER")