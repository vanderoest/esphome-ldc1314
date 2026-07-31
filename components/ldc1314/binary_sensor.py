import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM

from . import CONF_LDC1314_ID, LDC1314Component
from .sensor import CONF_CHANNEL, MAX_CHANNEL_INDEX

DEPENDENCIES = ["ldc1314"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
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
