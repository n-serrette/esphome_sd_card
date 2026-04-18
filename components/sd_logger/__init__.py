import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_comp
from esphome.components import binary_sensor as binary_sensor_comp
from esphome.components import sensor as sensor_comp
from esphome.components import text_sensor as text_sensor_comp
from esphome.const import CONF_ID
from .. import sd_card

DEPENDENCIES = ["sd_card"]
AUTO_LOAD = ["binary_sensor"]

CONF_SD_LOGGER_ID = "sd_logger_id"

sd_logger_ns = cg.esphome_ns.namespace("sd_logger")
SdLogger = sd_logger_ns.class_("SdLogger", cg.Component)

# ── Top-level keys ────────────────────────────────────────────────────────────
CONF_TIME_ID              = "time_id"
CONF_PATH                 = "path"
CONF_QUEUE_SIZE           = "queue_size"
CONF_TASK_PRIORITY        = "task_priority"
CONF_UPLOAD_URL           = "upload_url"
CONF_BEARER_TOKEN         = "bearer_token"
CONF_BACKOFF_INITIAL      = "backoff_initial"
CONF_BACKOFF_MAX          = "backoff_max"
CONF_PING_URL             = "ping_url"
CONF_PING_INTERVAL        = "ping_interval"
CONF_PING_TIMEOUT         = "ping_timeout"
CONF_FSYNC_INTERVAL       = "fsync_interval"
CONF_SYNC_ONLINE          = "sync_online"
CONF_SYNC_SENDING_BACKLOG = "sync_sending_backlog"

# ── logs: list keys ───────────────────────────────────────────────────────────
CONF_LOGS         = "logs"
CONF_NAME         = "name"
CONF_FOLDER       = "folder"
CONF_FILE_PREFIX  = "file_prefix"
CONF_HEADER       = "header"
CONF_LOG_INTERVAL = "log_interval"
CONF_ROTATION     = "rotation"
CONF_MAX_FILE_SIZE = "max_file_size"
CONF_SENSORS      = "sensors"
CONF_TEXT_SENSORS = "text_sensors"
CONF_SENSOR_ID    = "sensor_id"
CONF_FORMAT       = "format"

ROTATION_OPTIONS = {"daily": 0, "size": 1}

# ── Slot schemas — typed separately so ESPHome can resolve IDs unambiguously ───
NUMERIC_SLOT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR_ID): cv.use_id(sensor_comp.Sensor),
        cv.Optional(CONF_FORMAT, default="%.4f"): cv.string_strict,
    }
)

TEXT_SLOT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR_ID): cv.use_id(text_sensor_comp.TextSensor),
    }
)

# ── Per-log schema ─────────────────────────────────────────────────────────────
# Column order in the CSV: sensors: slots first (in order), then text_sensors:
# slots (in order).
LOG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NAME, default=""):           cv.string,
        cv.Optional(CONF_FOLDER, default=""):         cv.string,
        cv.Required(CONF_FILE_PREFIX):                cv.string_strict,
        cv.Optional(CONF_HEADER, default=""):         cv.string,
        cv.Required(CONF_LOG_INTERVAL):               cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ROTATION, default="daily"):  cv.one_of(*ROTATION_OPTIONS, lower=True),
        cv.Optional(CONF_MAX_FILE_SIZE, default=52428800): cv.int_range(min=1024),
        cv.Optional(CONF_SENSORS, default=[]):        cv.ensure_list(NUMERIC_SLOT_SCHEMA),
        cv.Optional(CONF_TEXT_SENSORS, default=[]):   cv.ensure_list(TEXT_SLOT_SCHEMA),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdLogger),

        # Hardware + time
        cv.Required(sd_card.CONF_SD_CARD_ID): cv.use_id(sd_card.SdCard),
        cv.Required(CONF_TIME_ID): cv.use_id(time_comp.RealTimeClock),

        # Base path on SD card for all logs (default: "logs")
        cv.Optional(CONF_PATH, default="logs"): cv.string_strict,

        # FreeRTOS tuning
        cv.Optional(CONF_QUEUE_SIZE, default=50): cv.int_range(min=5, max=200),
        cv.Optional(CONF_TASK_PRIORITY, default=1): cv.int_range(min=0, max=5),
        cv.Optional(CONF_FSYNC_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,

        # Cloud upload — all optional; omit upload_url to disable
        cv.Optional(CONF_UPLOAD_URL, default=""): cv.string,
        cv.Optional(CONF_BEARER_TOKEN, default=""): cv.string,
        cv.Optional(CONF_BACKOFF_INITIAL, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_BACKOFF_MAX, default="15min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PING_URL, default=""): cv.string,
        cv.Optional(CONF_PING_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PING_TIMEOUT, default="3s"): cv.positive_time_period_milliseconds,

        # Optional binary status sensors
        cv.Optional(CONF_SYNC_ONLINE): binary_sensor_comp.binary_sensor_schema().extend(
            {cv.Optional("name", default="Sync Online"): cv.string}
        ),
        cv.Optional(CONF_SYNC_SENDING_BACKLOG): binary_sensor_comp.binary_sensor_schema().extend(
            {cv.Optional("name", default="Sync Sending Backlog"): cv.string}
        ),

        # Log definitions
        cv.Optional(CONF_LOGS, default=[]): cv.ensure_list(LOG_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sd = await cg.get_variable(config[sd_card.CONF_SD_CARD_ID])
    cg.add(var.set_sd_card(sd))

    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))

    cg.add(var.set_queue_size(config[CONF_QUEUE_SIZE]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_fsync_interval_ms(config[CONF_FSYNC_INTERVAL].total_milliseconds))
    cg.add(var.set_path(config[CONF_PATH]))

    if config[CONF_UPLOAD_URL]:
        cg.add(var.set_upload_url(config[CONF_UPLOAD_URL]))
    if config[CONF_BEARER_TOKEN]:
        cg.add(var.set_bearer_token(config[CONF_BEARER_TOKEN]))
    cg.add(var.set_backoff_initial_ms(config[CONF_BACKOFF_INITIAL]))
    cg.add(var.set_backoff_max_ms(config[CONF_BACKOFF_MAX]))
    if config[CONF_PING_URL]:
        cg.add(var.set_ping_url(config[CONF_PING_URL]))
    cg.add(var.set_ping_interval_ms(config[CONF_PING_INTERVAL].total_milliseconds))
    cg.add(var.set_ping_timeout_ms(config[CONF_PING_TIMEOUT].total_milliseconds))

    if CONF_SYNC_ONLINE in config:
        bs = await binary_sensor_comp.new_binary_sensor(config[CONF_SYNC_ONLINE])
        cg.add(var.set_sync_online_binary_sensor(bs))

    if CONF_SYNC_SENDING_BACKLOG in config:
        bs2 = await binary_sensor_comp.new_binary_sensor(config[CONF_SYNC_SENDING_BACKLOG])
        cg.add(var.set_sync_sending_backlog_binary_sensor(bs2))

    # ── Register logs ────────────────────────────────────────────────────────
    for log_cfg in config.get(CONF_LOGS, []):
        rot = ROTATION_OPTIONS[log_cfg[CONF_ROTATION]]
        cg.add(
            var.begin_log(
                log_cfg[CONF_NAME],
                log_cfg[CONF_FOLDER],
                log_cfg[CONF_FILE_PREFIX],
                log_cfg[CONF_HEADER],
                log_cfg[CONF_LOG_INTERVAL].total_milliseconds,
                rot,
                log_cfg[CONF_MAX_FILE_SIZE],
            )
        )
        for slot in log_cfg.get(CONF_SENSORS, []):
            s = await cg.get_variable(slot[CONF_SENSOR_ID])
            cg.add(var.add_log_numeric_slot(s, slot[CONF_FORMAT]))
        for slot in log_cfg.get(CONF_TEXT_SENSORS, []):
            ts = await cg.get_variable(slot[CONF_SENSOR_ID])
            cg.add(var.add_log_text_slot(ts))
        cg.add(var.finalize_log())
