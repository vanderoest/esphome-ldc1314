import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import (
    CONF_CHANNEL,
    CONF_LDC1314_ID,
    CONF_OUTPUT_GAIN,
    MAX_CHANNEL_INDEX,
    LDC1314Component,
    ldc1314_ns,
)

DEPENDENCIES = ["ldc1314"]

CONF_IDRIVE = "idrive"
CONF_OFFSET = "offset"

LDC1314Number = ldc1314_ns.class_("LDC1314Number", number.Number, cg.Component)
LDC1314NumberField = ldc1314_ns.enum("LDC1314NumberField")


def _validate_ldc1314_number(config):
    has_channel_fields = CONF_IDRIVE in config or CONF_OFFSET in config
    has_gain = CONF_OUTPUT_GAIN in config
    if has_gain and has_channel_fields:
        raise cv.Invalid(
            f"'{CONF_OUTPUT_GAIN}' is a hub-level value and cannot be combined with "
            f"'{CONF_IDRIVE}'/'{CONF_OFFSET}' in the same number: entry"
        )
    if has_channel_fields and CONF_CHANNEL not in config:
        raise cv.Invalid(f"'{CONF_CHANNEL}' is required when '{CONF_IDRIVE}' or '{CONF_OFFSET}' is set")
    if has_gain and CONF_CHANNEL in config:
        raise cv.Invalid(f"'{CONF_CHANNEL}' must not be set for an '{CONF_OUTPUT_GAIN}' number")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_LDC1314_ID): cv.use_id(LDC1314Component),
            cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=MAX_CHANNEL_INDEX),
            cv.Optional(CONF_IDRIVE): number.number_schema(
                LDC1314Number,
                icon="mdi:current-dc",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_OFFSET): number.number_schema(
                LDC1314Number,
                icon="mdi:tune-variant",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_OUTPUT_GAIN): number.number_schema(
                LDC1314Number,
                icon="mdi:sine-wave",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_IDRIVE, CONF_OFFSET, CONF_OUTPUT_GAIN),
    _validate_ldc1314_number,
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LDC1314_ID])

    if idrive_config := config.get(CONF_IDRIVE):
        var = await number.new_number(idrive_config, min_value=0, max_value=31, step=1)
        await cg.register_parented(var, hub)
        cg.add(var.set_field(LDC1314NumberField.LDC1314_NUMBER_IDRIVE))
        cg.add(var.set_channel(config[CONF_CHANNEL]))

    if offset_config := config.get(CONF_OFFSET):
        var = await number.new_number(offset_config, min_value=0, max_value=65535, step=1)
        await cg.register_parented(var, hub)
        cg.add(var.set_field(LDC1314NumberField.LDC1314_NUMBER_OFFSET))
        cg.add(var.set_channel(config[CONF_CHANNEL]))

    # Only 1/4/8/16 are valid OUTPUT_GAIN settings; other values snap to 1x in the firmware (see
    # ldc1314_number.cpp) since the register field has exactly four legal states.
    if gain_config := config.get(CONF_OUTPUT_GAIN):
        var = await number.new_number(gain_config, min_value=1, max_value=16, step=1)
        await cg.register_parented(var, hub)
        cg.add(var.set_field(LDC1314NumberField.LDC1314_NUMBER_OUTPUT_GAIN))
