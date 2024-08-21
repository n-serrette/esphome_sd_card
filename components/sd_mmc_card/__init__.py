import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import (
    CONF_ID,
    CONF_DATA,
    CONF_PATH,
    CONF_CLK_PIN,
    CONF_INPUT,
    CONF_OUTPUT,
)
from esphome.core import CORE

CONF_SD_MMC_CARD_ID = "sd_mmc_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"

sd_mmc_card_component_ns = cg.esphome_ns.namespace("sd_mmc_card")
SdMmc = sd_mmc_card_component_ns.class_("SdMmc", cg.Component)

# Action
SdMmcWriteFileAction = sd_mmc_card_component_ns.class_("SdMmcWriteFileAction", automation.Action)
SdMmcAppendFileAction = sd_mmc_card_component_ns.class_("SdMmcAppendFileAction", automation.Action)
SdMmcCreateDirectoryAction = sd_mmc_card_component_ns.class_("SdMmcCreateDirectoryAction", automation.Action)
SdMmcRemoveDirectoryAction = sd_mmc_card_component_ns.class_("SdMmcRemoveDirectoryAction", automation.Action)
SdMmcDeleteFileAction = sd_mmc_card_component_ns.class_("SdMmcDeleteFileAction", automation.Action)

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
        cv.GenerateID(): cv.declare_id(SdMmc),
        cv.Optional(CONF_CLK_PIN, default="GPIO14"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_CMD_PIN, default="GPIO15"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DATA0_PIN, default="GPIO2"): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA1_PIN, default="GPIO4"): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA2_PIN, default="GPIO12"): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA3_PIN, default="GPIO13"): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))

    cmd = await cg.gpio_pin_expression(config[CONF_CMD_PIN])
    cg.add(var.set_cmd_pin(cmd))

    data0 = await cg.gpio_pin_expression(config[CONF_DATA0_PIN])
    cg.add(var.set_data0_pin(data0))

    data1 = await cg.gpio_pin_expression(config[CONF_DATA1_PIN])
    cg.add(var.set_data1_pin(data1))

    data2 = await cg.gpio_pin_expression(config[CONF_DATA2_PIN])
    cg.add(var.set_data2_pin(data2))

    data3 = await cg.gpio_pin_expression(config[CONF_DATA3_PIN])
    cg.add(var.set_data3_pin(data3))

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))

    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("FS", None)
            cg.add_library("SD_MMC", None)


SD_MMC_PATH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdMmc),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
    }
)

SD_MMC_WRITE_FILE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdMmc),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    }
).extend(SD_MMC_PATH_ACTION_SCHEMA)

@automation.register_action(
    "sd_mmc_card.write_file", SdMmcWriteFileAction, SD_MMC_WRITE_FILE_ACTION_SCHEMA
)
async def sd_mmc_write_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var


@automation.register_action(
    "sd_mmc_card.append_file", SdMmcAppendFileAction, SD_MMC_WRITE_FILE_ACTION_SCHEMA
)
async def sd_mmc_append_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var


@automation.register_action(
    "sd_mmc_card.create_directory", SdMmcCreateDirectoryAction, SD_MMC_PATH_ACTION_SCHEMA
)
async def sd_mmc_create_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var


@automation.register_action(
    "sd_mmc_card.remove_directory", SdMmcRemoveDirectoryAction, SD_MMC_PATH_ACTION_SCHEMA
)
async def sd_mmc_remove_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var


@automation.register_action(
    "sd_mmc_card.delete_file", SdMmcDeleteFileAction, SD_MMC_PATH_ACTION_SCHEMA
)
async def sd_mmc_delete_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var