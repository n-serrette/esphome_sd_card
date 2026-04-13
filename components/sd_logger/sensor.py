import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from . import SdLogger, CONF_SD_LOGGER_ID

# Config keys for this platform
CONF_SENSOR_ID             = "sensor_id"
CONF_FILE_PREFIX           = "file_prefix"
CONF_LOG_INTERVAL          = "log_interval"
CONF_HEADER                = "header"
CONF_FORMAT                = "format"
CONF_ROTATION              = "rotation"
CONF_MAX_FILE_SIZE         = "max_file_size"
CONF_FORCE_WRITE_ON_CHANGE = "force_write_on_change"

# Maps YAML string to the uint8_t value of RotationPolicy in C++
ROTATION_OPTIONS = {"daily": 0, "size": 1}

CONFIG_SCHEMA = cv.Schema(
    {
        # Auto-detected from the single sd_logger hub; specify if using multiple.
        cv.GenerateID(CONF_SD_LOGGER_ID): cv.use_id(SdLogger),
        # Reference to an EXISTING numeric sensor already declared in the config.
        cv.Required(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
        cv.Required(CONF_FILE_PREFIX): cv.string_strict,
        cv.Required(CONF_LOG_INTERVAL): cv.positive_time_period_milliseconds,
        cv.Required(CONF_HEADER): cv.string_strict,
        cv.Optional(CONF_FORMAT, default="%.4f"): cv.string_strict,
        cv.Optional(CONF_ROTATION, default="daily"): cv.one_of(*ROTATION_OPTIONS, lower=True),
        # Default 50 MB; only evaluated when rotation: size
        cv.Optional(CONF_MAX_FILE_SIZE, default=52428800): cv.int_range(min=1024),
        cv.Optional(CONF_FORCE_WRITE_ON_CHANGE, default=False): cv.boolean,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SD_LOGGER_ID])
    sens = await cg.get_variable(config[CONF_SENSOR_ID])
    rotation_val = ROTATION_OPTIONS[config[CONF_ROTATION]]
    cg.add(
        hub.add_numeric_sink(
            sens,
            config[CONF_FILE_PREFIX],
            config[CONF_HEADER],
            config[CONF_FORMAT],
            config[CONF_LOG_INTERVAL].total_milliseconds,
            config[CONF_FORCE_WRITE_ON_CHANGE],
            rotation_val,
            config[CONF_MAX_FILE_SIZE],
        )
    )
