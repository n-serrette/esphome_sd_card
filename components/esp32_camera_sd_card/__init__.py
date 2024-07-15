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

CONF_ESP32_CAMERA_SD_CARD_ID = "esp32_camera_sd_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"

esp32_camera_sd_card_component_ns = cg.esphome_ns.namespace("esp32_camera_sd_card")
Esp32CameraSDCardComponent = esp32_camera_sd_card_component_ns.class_("Esp32CameraSDCardComponent", cg.Component)

# Action
SDCardWriteFileAction = esp32_camera_sd_card_component_ns.class_("SDCardWriteFileAction", automation.Action)
SDCardAppendFileAction = esp32_camera_sd_card_component_ns.class_("SDCardAppendFileAction", automation.Action)
SDCardCreateDirectoryAction = esp32_camera_sd_card_component_ns.class_("SDCardCreateDirectoryAction", automation.Action)
SDCardRemoveDirectoryAction = esp32_camera_sd_card_component_ns.class_("SDCardRemoveDirectoryAction", automation.Action)
SDCardDeleteFileAction = esp32_camera_sd_card_component_ns.class_("SDCardDeleteFileAction", automation.Action)

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
        cv.Optional(CONF_CLK_PIN, default="GPIO14"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_CMD_PIN, default="GPIO15"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DATA0_PIN, default="GPIO2"): pins.gpio_pin_schema({CONF_OUTPUT: True, CONF_INPUT: True}),
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

    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("FS", None)
            cg.add_library("SD_MMC", None)

SD_CARD_PATH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Esp32CameraSDCardComponent),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
    }
)

SD_CARD_WRITE_FILE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Esp32CameraSDCardComponent),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    }
).extend(SD_CARD_PATH_ACTION_SCHEMA)

@automation.register_action(
    "esp32_camera_sd_card.write_file", SDCardWriteFileAction, SD_CARD_WRITE_FILE_ACTION_SCHEMA
)
async def esp32_camera_sd_card_write_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var


@automation.register_action(
    "esp32_camera_sd_card.append_file", SDCardAppendFileAction, SD_CARD_WRITE_FILE_ACTION_SCHEMA
)
async def esp32_camera_sd_card_append_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var


@automation.register_action(
    "esp32_camera_sd_card.create_directory", SDCardCreateDirectoryAction, SD_CARD_PATH_ACTION_SCHEMA
)
async def esp32_camera_sd_card_create_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var


@automation.register_action(
    "esp32_camera_sd_card.remove_directory", SDCardRemoveDirectoryAction, SD_CARD_PATH_ACTION_SCHEMA
)
async def esp32_camera_sd_card_remove_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var


@automation.register_action(
    "esp32_camera_sd_card.delete_file", SDCardDeleteFileAction, SD_CARD_PATH_ACTION_SCHEMA
)
async def esp32_camera_sd_card_delete_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var