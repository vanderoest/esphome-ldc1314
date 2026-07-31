import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_LDC1314_ID, LDC1314Component, ldc1314_ns

DEPENDENCIES = ["ldc1314"]

CONF_CHARACTERIZE = "characterize"
CONF_CLEAR_CHARACTERIZATION = "clear_characterization"
CONF_CLEAR_OVERRIDES = "clear_overrides"

LDC1314Button = ldc1314_ns.class_("LDC1314Button", button.Button)
LDC1314ButtonAction = ldc1314_ns.enum("LDC1314ButtonAction")

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
            cv.Optional(CONF_CHARACTERIZE): button.button_schema(
                LDC1314Button,
                icon="mdi:tune-variant",
            ),
            cv.Optional(CONF_CLEAR_CHARACTERIZATION): button.button_schema(
                LDC1314Button,
                icon="mdi:delete-sweep",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_CLEAR_OVERRIDES): button.button_schema(
                LDC1314Button,
                icon="mdi:restore",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
        }
    ),
    cv.has_at_least_one_key(
        CONF_CHARACTERIZE, CONF_CLEAR_CHARACTERIZATION, CONF_CLEAR_OVERRIDES
    ),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LDC1314_ID])

    if characterize_config := config.get(CONF_CHARACTERIZE):
        var = await button.new_button(characterize_config)
        await cg.register_parented(var, hub)
        cg.add(var.set_action(LDC1314ButtonAction.LDC1314_BUTTON_CHARACTERIZE))

    if clear_cal_config := config.get(CONF_CLEAR_CHARACTERIZATION):
        var = await button.new_button(clear_cal_config)
        await cg.register_parented(var, hub)
        cg.add(var.set_action(LDC1314ButtonAction.LDC1314_BUTTON_CLEAR_CHARACTERIZATION))

    if clear_ovr_config := config.get(CONF_CLEAR_OVERRIDES):
        var = await button.new_button(clear_ovr_config)
        await cg.register_parented(var, hub)
        cg.add(var.set_action(LDC1314ButtonAction.LDC1314_BUTTON_CLEAR_OVERRIDES))
