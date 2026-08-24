import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_CHANNEL, CONF_LDC1314_ID, MAX_CHANNEL_INDEX, LDC1314Component

DEPENDENCIES = ["ldc1314"]

# Diagnostic, not a primary entity: this reports the health of a channel, never a measurement.
# Home Assistant files it under the device's Diagnostics section instead of among the real
# entities, while keeping it fully usable in automations -- which matters, because since the
# read-failure path started raising this flag it is the only entity that surfaces a dead I2C bus
# at all. A frozen volume reading looks identical to "nobody used water today"; this doesn't.
CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=MAX_CHANNEL_INDEX),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_LDC1314_ID])
    cg.add(hub.set_channel_error_binary_sensor(config[CONF_CHANNEL], var))
