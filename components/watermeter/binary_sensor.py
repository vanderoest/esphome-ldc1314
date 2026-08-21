import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_MOISTURE, DEVICE_CLASS_RUNNING
from esphome.core import ID

from . import CONF_WATERMETER_ID, WatermeterComponent

DEPENDENCIES = ["watermeter"]

CONF_FLOWING = "flowing"
CONF_CONTINUOUS_FLOW = "continuous_flow"
CONF_MIN_DURATION = "min_duration"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_WATERMETER_ID): cv.use_id(WatermeterComponent),
        cv.Optional(CONF_FLOWING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
        ),
        # min_duration is a friendlier front for the standard delayed_on filter (reused, not
        # reimplemented -- CLAUDE.md) rather than requiring the user to write
        # `filters: [delayed_on: ...]` themselves on a raw binary_sensor.
        cv.Optional(CONF_CONTINUOUS_FLOW): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOISTURE,
        ).extend(
            {
                cv.Optional(CONF_MIN_DURATION, default="30min"): cv.positive_time_period_milliseconds,
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_WATERMETER_ID])

    if flowing_config := config.get(CONF_FLOWING):
        sens = await binary_sensor.new_binary_sensor(flowing_config)
        cg.add(hub.set_flowing_binary_sensor(sens))

    if continuous_config := config.get(CONF_CONTINUOUS_FLOW):
        sens = await binary_sensor.new_binary_sensor(continuous_config)
        # An ID created here (codegen time) never passes through the schema-declaration
        # resolution pass that auto-names a bare ID(None, ...) -- that pass already finished by
        # now, over in config validation -- so it needs an explicit, deterministic name instead.
        filter_id = ID(
            f"{config[CONF_WATERMETER_ID]}_continuous_flow_filter",
            is_declaration=True,
            is_manual=True,
            type=binary_sensor.DelayedOnFilter,
        )
        filter_var = cg.new_Pvariable(filter_id)
        # set_delay() takes a TemplatableFn now, not a raw value -- matches how the stock
        # `delayed_on` filter's own to_code wraps its value (binary_sensor/__init__.py).
        delay = await cg.templatable(continuous_config[CONF_MIN_DURATION], [], cg.uint32)
        cg.add(filter_var.set_delay(delay))
        cg.add(sens.add_filters([filter_var]))
        cg.add_define("USE_BINARY_SENSOR_FILTER")
        cg.add(hub.set_continuous_flow_binary_sensor(sens))
