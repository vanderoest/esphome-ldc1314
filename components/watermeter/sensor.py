import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_WATER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    UNIT_DEGREES,
    UNIT_LITRE,
)

from . import CONF_WATERMETER_ID, WatermeterComponent

DEPENDENCIES = ["watermeter"]

CONF_VOLUME = "volume"
CONF_FLOW_RATE = "flow_rate"
CONF_ANGLE = "angle"
CONF_SIGNAL_QUALITY = "signal_quality"
CONF_REVOLUTIONS = "revolutions"
CONF_REVERSE_VOLUME = "reverse_volume"

# No UNIT_LITRE_PER_MINUTE in esphome.const (only _PER_HOUR/_PER_SECOND exist) -- unit_of_measurement
# accepts any string, so a local constant is the normal way to fill that gap.
UNIT_LITRE_PER_MINUTE = "L/min"

# All optional and independent -- pick whichever outputs you want, same shape as the driver's own
# per-channel sensor:/binary_sensor: blocks but as named sub-keys on one platform entry (the
# scd4x/sht4x shape from .plan's YAML contract) rather than a repeated list.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_WATERMETER_ID): cv.use_id(WatermeterComponent),
        # state_class: total, not total_increasing -- see design_decisions.md /
        # .plan "Home Assistant semantics" for why: a meter that subtracts reverse flow can
        # decrease, and total_increasing reads any decrease as a meter replacement.
        cv.Optional(CONF_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_LITRE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL,
        ),
        # device_class: volume_flow_rate, not water -- HA's Energy Dashboard's optional water
        # flow-rate field validates against this device class specifically (distinct from the
        # cumulative device_class: water above), which the unset-device_class version of this
        # sensor was rejected by ("Unexpected device class" on the entity).
        cv.Optional(CONF_FLOW_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LITRE_PER_MINUTE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ANGLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SIGNAL_QUALITY): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_REVOLUTIONS): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_TOTAL,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_REVERSE_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_LITRE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

SENSOR_MAP = {
    CONF_VOLUME: "set_volume_sensor",
    CONF_FLOW_RATE: "set_flow_rate_sensor",
    CONF_ANGLE: "set_angle_sensor",
    CONF_SIGNAL_QUALITY: "set_signal_quality_sensor",
    CONF_REVOLUTIONS: "set_revolutions_sensor",
    CONF_REVERSE_VOLUME: "set_reverse_volume_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_WATERMETER_ID])

    for key, func_name in SENSOR_MAP.items():
        if sub_config := config.get(key):
            sens = await sensor.new_sensor(sub_config)
            cg.add(getattr(hub, func_name)(sens))
