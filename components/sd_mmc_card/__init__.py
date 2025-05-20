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
    CONF_PULLUP,
    CONF_PULLDOWN,
    CONF_TYPE,
    CONF_SPI,
)
from esphome.core import CORE
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32S3,
    VARIANT_ESP32C6,
)
from esphome.components import spi
import esphome.final_validate as fv
from esphome.components import esp32

DEPENDENCIES = ["esp32"]

CONF_SD_MMC_CARD_ID = "sd_mmc_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_POWER_CTRL_PIN = "power_ctrl_pin"
CONF_SPI_INTERFACE = "_spi_interface"

sd_mmc_card_component_ns = cg.esphome_ns.namespace("sd_mmc_card")
SdCard = sd_mmc_card_component_ns.class_("SdCard")
SdMmc = sd_mmc_card_component_ns.class_("SdMmc", cg.Component, SdCard)
SdSpi = sd_mmc_card_component_ns.class_("SdSpi", spi.SPIDevice, cg.Component, SdCard)

TYPE_SD_MMC = "sd_mmc"
TYPE_SD_SPI = "sd_spi"

TYPE_CLASS = {
    TYPE_SD_MMC: SdMmc,
    TYPE_SD_SPI: SdSpi,
}

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

SD_MMC_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdMmc),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
        cv.Optional(CONF_POWER_CTRL_PIN) : pins.gpio_pin_schema({
            CONF_OUTPUT: True,
            CONF_PULLUP: False,
            CONF_PULLDOWN: False,
        }),
    }
).extend(cv.COMPONENT_SCHEMA)

def validate_spi_cs_config(config):
    data3_pin_config = config.get(CONF_DATA3_PIN)
    cs_pin_config = config.get(spi.CONF_CS_PIN)
    if data3_pin_config and cs_pin_config:
        raise cv.Invalid(f"{CONF_DATA3_PIN} is the same as {spi.CONF_CS_PIN}. Please remove one.")
    if not data3_pin_config and not cs_pin_config:
        raise cv.Invalid(f"{CONF_DATA3_PIN} or {spi.CONF_CS_PIN} required. Please specify one.")
    if data3_pin_config:
        config[spi.CONF_CS_PIN] = data3_pin_config
        del config[CONF_DATA3_PIN]
    return config

def validate_spi_bus_pins(config):
    cmd_pin = config.get(CONF_CMD_PIN)
    data0_pin = config.get(CONF_DATA0_PIN)
    clk_pin = config.get(CONF_CLK_PIN)
    if cmd_pin or data0_pin or clk_pin:
        raise cv.Invalid(f"Please move pins to SPI bus definition:\n '{CONF_CMD_PIN}' to '{spi.CONF_MOSI_PIN}'\n '{CONF_DATA0_PIN}' to '{spi.CONF_MISO_PIN}'\n '{CONF_CLK_PIN}' to '{spi.CONF_CLK_PIN}'")
    return config

def validate_spi_mode(config):
    if not CORE.using_esp_idf:
        raise cv.Invalid("Only esp-idf supported for SD SPI")
    if config[CONF_MODE_1BIT] == False:
        raise cv.Invalid("Only 1bit mode supported for SPI")
    return config

SD_SPI_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdSpi),
        cv.Optional(CONF_MODE_1BIT, default=True): cv.boolean,
        cv.Optional(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_DATA0_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_DATA3_PIN): pins.gpio_output_pin_schema, # alias for cs pin
        # unused pins in case they are connecter set to pull up mode
        cv.Optional(CONF_DATA1_PIN): pins.gpio_input_pullup_pin_schema,
        cv.Optional(CONF_DATA2_PIN): pins.gpio_input_pullup_pin_schema,
    },
    extra_schemas=[
        validate_spi_cs_config,
        validate_spi_bus_pins,
        validate_spi_mode,
    ]
).extend(spi.spi_device_schema(cs_pin_required=False))

def validate_config(config):
    variant = get_esp32_variant()
    if variant == VARIANT_ESP32C6 and config.get(CONF_TYPE) != TYPE_SD_SPI:
        raise cv.Invalid(f"esp32c6 doesn't have sdmmc host support. Please use `type: sd_spi`")

    if not CORE.is_esp32:
        return
    if variant not in [VARIANT_ESP32, VARIANT_ESP32S3, VARIANT_ESP32C6]:
        raise cv.Invalid(f"Unsupported variant {variant}")

    return config

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            TYPE_SD_MMC: SD_MMC_SCHEMA,
            TYPE_SD_SPI: SD_SPI_SCHEMA,
        },
        default_type=TYPE_SD_MMC,
    ),
    validate_config,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if mode_1bit := config.get(CONF_MODE_1BIT):
        cg.add(var.set_mode_1bit(mode_1bit))
    if clk_pin := config.get(CONF_CLK_PIN):
        cg.add(var.set_clk_pin(clk_pin))
    if cmd_pin := config.get(CONF_CMD_PIN):
        cg.add(var.set_cmd_pin(cmd_pin))
    if data0_pin := config.get(CONF_DATA0_PIN):
        cg.add(var.set_data0_pin(data0_pin))

    if power_ctrl_pin := config.get(CONF_POWER_CTRL_PIN):
        power_ctrl = await cg.gpio_pin_expression(power_ctrl_pin)
        cg.add(var.set_power_ctrl_pin(power_ctrl))

    if spi_interface := config.get(CONF_SPI_INTERFACE):
        cg.add(var.set_spi_interface(cg.RawExpression(spi_interface)))

    if config[CONF_TYPE] == TYPE_SD_SPI:
        cg.add_define("SDMMC_USE_SDSPI")
        # cg.add_library("esp_driver_sdspi", None)
        # cg.add_library("sdmmc", None)
        await spi.register_spi_device(var, config)
        if pin := config.get(CONF_DATA1_PIN):
            cg.add(var.set_data1_pin(await cg.gpio_pin_expression(pin)))
        if pin := config.get(CONF_DATA2_PIN):
            cg.add(var.set_data2_pin(await cg.gpio_pin_expression(pin)))

    elif config[CONF_TYPE] == TYPE_SD_MMC:
        cg.add_define("SDMMC_USE_SDMMC")
        if (config[CONF_MODE_1BIT] == False):
            cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
            cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
            cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))



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

def _final_validate(config):
    spi_id = config[spi.CONF_SPI_ID]
    if config[CONF_TYPE] != TYPE_SD_SPI:
        return
    if spi_configs := fv.full_config.get().get(CONF_SPI):
        for spi_conf in spi_configs:
            if spi_conf[spi.CONF_ID] != spi_id:
                continue
            index = spi_conf.get(spi.CONF_INTERFACE_INDEX)
            if index is None:
                raise cv.Invalid(f"Can't find interface index in spi config {spi_id}")
            
            interface = spi.get_spi_interface(index)
            config[CONF_SPI_INTERFACE] = interface


FINAL_VALIDATE_SCHEMA = _final_validate

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
