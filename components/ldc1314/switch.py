import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_LDC1314_ID, LDC1314Component, ldc1314_ns

DEPENDENCIES = ["ldc1314"]

LDC1314TraceSwitch = ldc1314_ns.class_("LDC1314TraceSwitch", switch.Switch, cg.Component)

# Single-entity platform: turning it on enables the TRACE,<ts>,<ch0>,<ch1>,<ch2> log line
# (ESP_LOGD) on every update() cycle -- for Phase A trace capture (.plan Part 1, "Add a raw-trace
# log line"). ALWAYS_OFF: a capture session is something started deliberately each boot, not
# something that should silently resume after a reset.
CONFIG_SCHEMA = switch.switch_schema(
    LDC1314TraceSwitch,
    default_restore_mode="ALWAYS_OFF",
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:chart-timeline-variant",
).extend(
    {
        cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_LDC1314_ID])
