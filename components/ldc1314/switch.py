import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_LDC1314_ID, LDC1314Component, ldc1314_ns

DEPENDENCIES = ["ldc1314"]

CONF_OVERRIDE = "override"

LDC1314Switch = ldc1314_ns.class_("LDC1314Switch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
        cv.Required(CONF_OVERRIDE): switch.switch_schema(
            LDC1314Switch,
            icon="mdi:tune-vertical-variant",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LDC1314_ID])
    var = await switch.new_switch(config[CONF_OVERRIDE])
    await cg.register_parented(var, hub)
