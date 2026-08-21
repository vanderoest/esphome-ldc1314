import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_WATERMETER_ID, WatermeterComponent, watermeter_ns

DEPENDENCIES = ["watermeter"]

CONF_DURATION = "duration"

WatermeterLearnButton = watermeter_ns.class_("WatermeterLearnButton", button.Button)

# Single-entity platform: pressing it starts a learn pass -- rotate the meter through exactly one
# known revolution over the next `duration`, and the hub captures per-phase mid/amp from whatever
# it sees, replacing the free-running envelope follower's own estimate. See README once written /
# .plan "Normalise".
CONFIG_SCHEMA = button.button_schema(
    WatermeterLearnButton,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:tape-measure",
).extend(
    {
        cv.GenerateID(CONF_WATERMETER_ID): cv.use_id(WatermeterComponent),
        cv.Optional(CONF_DURATION, default="10s"): cv.positive_time_period_seconds,
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_WATERMETER_ID])
    cg.add(var.set_duration(config[CONF_DURATION].total_seconds))
