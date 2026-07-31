import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_LDC1314_ID, LDC1314Component, ldc1314_ns

DEPENDENCIES = ["ldc1314"]

LDC1314DiagnosticsButton = ldc1314_ns.class_(
    "LDC1314DiagnosticsButton", button.Button
)

# Single-entity platform: pressing it logs the datasheet-conformance report for the channels as
# they are configured right now. See README "Datasheet conformance report".
CONFIG_SCHEMA = button.button_schema(
    LDC1314DiagnosticsButton,
    icon="mdi:stethoscope",
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_LDC1314_ID])
