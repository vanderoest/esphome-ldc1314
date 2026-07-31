import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import STATE_CLASS_MEASUREMENT

from . import CONF_LDC1314_ID, LDC1314Component

DEPENDENCIES = ["ldc1314"]

CONF_CHANNEL = "channel"
CONF_RCOUNT = "rcount"
CONF_SETTLECOUNT = "settlecount"
CONF_OFFSET = "offset"
CONF_FIN_DIVIDER = "fin_divider"
CONF_FREF_DIVIDER = "fref_divider"
CONF_IDRIVE = "idrive"

# Matches the LDC1314's fixed 4-channel hardware limit (MAX_CHANNELS in ldc1314_registers.h).
# v1 targets the LDC1314 only -- see .plan "Device scope" / design_decisions.md.
MAX_CHANNEL_INDEX = 3

CONFIG_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=MAX_CHANNEL_INDEX),
        # Raw datasheet register values -- see register_map.md for valid ranges/semantics.
        # RCOUNT 0x0000-0x0004 are reserved; 0x0080 is the device's own reset default.
        cv.Optional(CONF_RCOUNT, default=0x0080): cv.int_range(min=5, max=65535),
        cv.Optional(CONF_SETTLECOUNT, default=0x0000): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_OFFSET, default=0x0000): cv.int_range(min=0, max=65535),
        # FIN_DIVIDER 0 is reserved; must be >=2 if the sensor frequency is >=8.75MHz (datasheet).
        cv.Optional(CONF_FIN_DIVIDER, default=1): cv.int_range(min=1, max=15),
        # FREF_DIVIDER 0 is reserved.
        cv.Optional(CONF_FREF_DIVIDER, default=1): cv.int_range(min=1, max=1023),
        cv.Optional(CONF_IDRIVE, default=0): cv.int_range(min=0, max=31),
    }
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_LDC1314_ID])

    channel = config[CONF_CHANNEL]
    cg.add(hub.set_channel_sensor(channel, var))
    cg.add(hub.set_channel_rcount(channel, config[CONF_RCOUNT]))
    cg.add(hub.set_channel_settlecount(channel, config[CONF_SETTLECOUNT]))
    cg.add(hub.set_channel_offset(channel, config[CONF_OFFSET]))
    cg.add(hub.set_channel_fin_divider(channel, config[CONF_FIN_DIVIDER]))
    cg.add(hub.set_channel_fref_divider(channel, config[CONF_FREF_DIVIDER]))
    cg.add(hub.set_channel_idrive(channel, config[CONF_IDRIVE]))
