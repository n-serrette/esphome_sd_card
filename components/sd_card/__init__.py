import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import (
    CONF_ID,
    CONF_DATA,
    CONF_PATH,
    CONF_CLK_PIN,
    CONF_OUTPUT,
    CONF_PULLUP,
    CONF_PULLDOWN,
)
from esphome.core import CORE
from esphome.components.esp32 import get_esp32_variant, include_builtin_idf_component
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)

DEPENDENCIES = ["esp32"]

CONF_SD_CARD_ID = "sd_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_POWER_CTRL_PIN = "power_ctrl_pin"

sd_card_component_ns = cg.esphome_ns.namespace("sd_card")
SdCard = sd_card_component_ns.class_("SdCard", cg.Component)

# Action
SdCardWriteFileAction = sd_card_component_ns.class_("SdCardWriteFileAction", automation.Action)
SdCardAppendFileAction = sd_card_component_ns.class_("SdCardAppendFileAction", automation.Action)
SdCardCreateDirectoryAction = sd_card_component_ns.class_("SdCardCreateDirectoryAction", automation.Action)
SdCardRemoveDirectoryAction = sd_card_component_ns.class_("SdCardRemoveDirectoryAction", automation.Action)
SdCardDeleteFileAction = sd_card_component_ns.class_("SdCardDeleteFileAction", automation.Action)

def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )

CONFIG_SCHEMA = cv.All(
    cv.require_esphome_version(2025,7,0),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SdCard),
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
            cv.Optional(CONF_POWER_CTRL_PIN) : pins.gpio_pin_schema({
                CONF_OUTPUT: True,
                CONF_PULLUP: False,
                CONF_PULLDOWN: False,
            }),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    # fatfs and driver are excluded from the ESP-IDF build by default.
    # Re-include them so their headers and symbols are available.
    include_builtin_idf_component("fatfs")
    include_builtin_idf_component("driver")

    # fatfs LFN support: heap-based allocation + max 255-char filenames.
    # ffconf.h maps CONFIG_FATFS_LFN_HEAP -> FF_USE_LFN=3 and
    # CONFIG_FATFS_MAX_LFN -> FF_MAX_LFN. Both must be set when LFN is enabled;
    # ff.c raises #error if FF_MAX_LFN is undefined.
    cg.add_build_flag("-DCONFIG_FATFS_LFN_HEAP=1")
    cg.add_build_flag("-DCONFIG_FATFS_MAX_LFN=255")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))

    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    if (config[CONF_MODE_1BIT] == False):
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    if (CONF_POWER_CTRL_PIN in config):
        power_ctrl = await cg.gpio_pin_expression(config[CONF_POWER_CTRL_PIN])
        cg.add(var.set_power_ctrl_pin(power_ctrl))


def _final_validate(_):
    if not CORE.is_esp32:
        return
    variant = get_esp32_variant()
    if variant not in [VARIANT_ESP32, VARIANT_ESP32S3]:
        raise cv.Invalid(f"Unsupported variant {variant}")


FINAL_VALIDATE_SCHEMA = _final_validate


SD_CARD_PATH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdCard),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
    }
)

SD_CARD_WRITE_FILE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdCard),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    }
).extend(SD_CARD_PATH_ACTION_SCHEMA)

@automation.register_action(
    "sd_card.write_file", SdCardWriteFileAction, SD_CARD_WRITE_FILE_ACTION_SCHEMA, synchronous=True
)
async def sd_card_write_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var


@automation.register_action(
    "sd_card.append_file", SdCardAppendFileAction, SD_CARD_WRITE_FILE_ACTION_SCHEMA, synchronous=True
)
async def sd_card_append_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var


@automation.register_action(
    "sd_card.create_directory", SdCardCreateDirectoryAction, SD_CARD_PATH_ACTION_SCHEMA, synchronous=True
)
async def sd_card_create_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var


@automation.register_action(
    "sd_card.remove_directory", SdCardRemoveDirectoryAction, SD_CARD_PATH_ACTION_SCHEMA, synchronous=True
)
async def sd_card_remove_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var


@automation.register_action(
    "sd_card.delete_file", SdCardDeleteFileAction, SD_CARD_PATH_ACTION_SCHEMA, synchronous=True
)
async def sd_card_delete_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var
