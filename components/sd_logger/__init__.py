import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_comp
from esphome.components import binary_sensor as binary_sensor_comp
from esphome.const import CONF_ID
from .. import sd_mmc

DEPENDENCIES = ["sd_mmc"]
AUTO_LOAD = ["binary_sensor"]

# Exported so sensor.py / text_sensor.py platform files can reference the hub.
CONF_SD_LOGGER_ID = "sd_logger_id"

sd_logger_ns = cg.esphome_ns.namespace("sd_logger")
SdLogger = sd_logger_ns.class_("SdLogger", cg.Component)

CONF_TIME_ID              = "time_id"
CONF_QUEUE_SIZE           = "queue_size"
CONF_TASK_PRIORITY        = "task_priority"
CONF_UPLOAD_URL           = "upload_url"
CONF_BEARER_TOKEN         = "bearer_token"
CONF_BACKOFF_INITIAL      = "backoff_initial"
CONF_BACKOFF_MAX          = "backoff_max"
CONF_PING_URL             = "ping_url"
CONF_PING_INTERVAL        = "ping_interval"
CONF_PING_TIMEOUT         = "ping_timeout"
CONF_FSYNC_INTERVAL        = "fsync_interval"
CONF_SYNC_ONLINE          = "sync_online"
CONF_SYNC_SENDING_BACKLOG = "sync_sending_backlog"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdLogger),

        # Hardware + time
        cv.Required(sd_mmc.CONF_SD_MMC_ID): cv.use_id(sd_mmc.SdMmc),
        cv.Required(CONF_TIME_ID): cv.use_id(time_comp.RealTimeClock),

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
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sd = await cg.get_variable(config[sd_mmc.CONF_SD_MMC_ID])
    cg.add(var.set_sd_mmc(sd))

    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))

    cg.add(var.set_queue_size(config[CONF_QUEUE_SIZE]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_fsync_interval_ms(config[CONF_FSYNC_INTERVAL].total_milliseconds))

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