from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
# Imported by name, not `from esphome.components import sensor`: this package has its own
# sensor.py sibling module, and once ESPHome's loader imports esphome.components.ldc1314.sensor
# for the `sensor:` platform, Python sets that submodule as the `sensor` attribute on THIS
# package's own namespace -- silently shadowing the top-level esphome.components.sensor import
# by the time to_code() below actually runs.
from esphome.components.sensor import new_sensor, sensor_schema
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

# Placeholder maintainer handle -- update if/when this component is proposed for esphome/esphome
# core.
CODEOWNERS = ["@vanderoest"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

ldc1314_ns = cg.esphome_ns.namespace("ldc1314")
LDC1314Component = ldc1314_ns.class_("LDC1314Component", cg.PollingComponent, i2c.I2CDevice)

CharacterizeAction = ldc1314_ns.class_("CharacterizeAction", automation.Action)
ClearCharacterizationAction = ldc1314_ns.class_("ClearCharacterizationAction", automation.Action)
ClearOverridesAction = ldc1314_ns.class_("ClearOverridesAction", automation.Action)
SetOverrideAction = ldc1314_ns.class_("SetOverrideAction", automation.Action)
IsCharacterizingCondition = ldc1314_ns.class_("IsCharacterizingCondition", automation.Condition)

ReferenceClockSource = ldc1314_ns.enum("ReferenceClockSource")
REFERENCE_CLOCK_SOURCES = {
    "INTERNAL": ReferenceClockSource.LDC1314_REFERENCE_CLOCK_INTERNAL,
    "EXTERNAL": ReferenceClockSource.LDC1314_REFERENCE_CLOCK_EXTERNAL,
}

Deglitch = ldc1314_ns.enum("Deglitch")
DEGLITCH_FREQUENCIES = {
    "1MHZ": Deglitch.LDC1314_DEGLITCH_1MHZ,
    "3_3MHZ": Deglitch.LDC1314_DEGLITCH_3_3MHZ,
    "10MHZ": Deglitch.LDC1314_DEGLITCH_10MHZ,
    "33MHZ": Deglitch.LDC1314_DEGLITCH_33MHZ,
}

OutputGain = ldc1314_ns.enum("OutputGain")
OUTPUT_GAINS = {
    1: OutputGain.LDC1314_OUTPUT_GAIN_1,
    4: OutputGain.LDC1314_OUTPUT_GAIN_4,
    8: OutputGain.LDC1314_OUTPUT_GAIN_8,
    16: OutputGain.LDC1314_OUTPUT_GAIN_16,
}

SensorActivationMode = ldc1314_ns.enum("SensorActivationMode")
SENSOR_ACTIVATION_MODES = {
    "FULL_CURRENT": SensorActivationMode.LDC1314_SENSOR_ACTIVATE_FULL_CURRENT,
    "LOW_POWER": SensorActivationMode.LDC1314_SENSOR_ACTIVATE_LOW_POWER,
}

CONF_LDC1314_ID = "ldc1314_id"
CONF_REFERENCE_CLOCK = "reference_clock"
CONF_DEGLITCH = "deglitch"
CONF_OUTPUT_GAIN = "output_gain"
CONF_HIGH_CURRENT_DRIVE = "high_current_drive"
CONF_SENSOR_ACTIVATION_MODE = "sensor_activation_mode"
CONF_REPORT_ERRORS_ON_INTB = "report_errors_on_intb"

# Hoisted out of sensor.py: binary_sensor.py, number.py and button.py all need these, and
# sensor.py was the wrong place for them to live once more than one platform needed them.
CONF_CHANNEL = "channel"
# Matches the LDC1314's fixed 4-channel hardware limit (MAX_CHANNELS in ldc1314_registers.h).
# v1 targets the LDC1314 only -- see design_decisions.md.
MAX_CHANNEL_INDEX = 3

# --- Characterization (see .plan "Public API") ------------------------------------------------

CONF_CHARACTERIZATION = "characterization"
CONF_RESTORE = "restore"
CONF_IDLE_DURATION = "idle_duration"
CONF_STAGE_DURATION = "stage_duration"
CONF_VERIFY_DURATION = "verify_duration"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_PROGRESS_INTERVAL = "progress_interval"
CONF_HEADROOM = "headroom"
CONF_UNIFY_CHANNELS = "unify_channels"
CONF_PROMPTS = "prompts"
CONF_PROMPT_START = "start"
CONF_PROMPT_STOP = "stop"
CONF_CALIBRATED_OUTPUT_GAIN = "calibrated_output_gain"
CONF_ARMED = "armed"

# Target-neutral defaults -- see .plan "Prompt wording is configurable, and that is what keeps
# the driver generic". A watermeter installation overrides these with tap-specific wording.
DEFAULT_PROMPT_START = "Start moving the target through its full range now."
DEFAULT_PROMPT_STOP = "You can stop moving the target now."

CHARACTERIZATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RESTORE, default=True): cv.boolean,
        cv.Optional(CONF_IDLE_DURATION, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_STAGE_DURATION, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_VERIFY_DURATION, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PROGRESS_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HEADROOM, default="25%"): cv.percentage,
        cv.Optional(CONF_UNIFY_CHANNELS, default=False): cv.boolean,
        cv.Optional(CONF_PROMPTS, default={}): cv.Schema(
            {
                cv.Optional(CONF_PROMPT_START, default=DEFAULT_PROMPT_START): cv.string,
                cv.Optional(CONF_PROMPT_STOP, default=DEFAULT_PROMPT_STOP): cv.string,
            }
        ),
        cv.Optional(CONF_CALIBRATED_OUTPUT_GAIN): sensor_schema(
            icon="mdi:tune-variant",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LDC1314Component),
            cv.Optional(CONF_REFERENCE_CLOCK, default="INTERNAL"): cv.enum(
                REFERENCE_CLOCK_SOURCES, upper=True
            ),
            cv.Optional(CONF_DEGLITCH, default="33MHZ"): cv.enum(
                DEGLITCH_FREQUENCIES, upper=True
            ),
            cv.Optional(CONF_OUTPUT_GAIN, default=1): cv.enum(OUTPUT_GAINS, int=True),
            cv.Optional(CONF_HIGH_CURRENT_DRIVE, default=False): cv.boolean,
            cv.Optional(CONF_SENSOR_ACTIVATION_MODE, default="FULL_CURRENT"): cv.enum(
                SENSOR_ACTIVATION_MODES, upper=True
            ),
            cv.Optional(CONF_REPORT_ERRORS_ON_INTB, default=False): cv.boolean,
            cv.Optional(CONF_CHARACTERIZATION, default={}): CHARACTERIZATION_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x2A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_reference_clock_source(config[CONF_REFERENCE_CLOCK]))
    cg.add(var.set_deglitch(config[CONF_DEGLITCH]))
    cg.add(var.set_output_gain(config[CONF_OUTPUT_GAIN]))
    cg.add(var.set_high_current_drive(config[CONF_HIGH_CURRENT_DRIVE]))
    cg.add(var.set_sensor_activation_mode(config[CONF_SENSOR_ACTIVATION_MODE]))
    cg.add(var.set_report_errors_on_intb(config[CONF_REPORT_ERRORS_ON_INTB]))

    char_conf = config[CONF_CHARACTERIZATION]
    cg.add(var.set_char_restore(char_conf[CONF_RESTORE]))
    cg.add(var.set_char_idle_duration(char_conf[CONF_IDLE_DURATION].total_milliseconds))
    cg.add(var.set_char_stage_duration(char_conf[CONF_STAGE_DURATION].total_milliseconds))
    cg.add(var.set_char_verify_duration(char_conf[CONF_VERIFY_DURATION].total_milliseconds))
    cg.add(var.set_char_sample_interval(char_conf[CONF_SAMPLE_INTERVAL].total_milliseconds))
    cg.add(var.set_char_progress_interval(char_conf[CONF_PROGRESS_INTERVAL].total_milliseconds))
    cg.add(var.set_char_headroom(char_conf[CONF_HEADROOM]))
    cg.add(var.set_char_unify_channels(char_conf[CONF_UNIFY_CHANNELS]))
    prompts = char_conf[CONF_PROMPTS]
    cg.add(var.set_char_prompt_start(prompts[CONF_PROMPT_START]))
    cg.add(var.set_char_prompt_stop(prompts[CONF_PROMPT_STOP]))

    if gain_sensor_config := char_conf.get(CONF_CALIBRATED_OUTPUT_GAIN):
        sens = await new_sensor(gain_sensor_config)
        cg.add(var.set_calibrated_output_gain_sensor(sens))


LDC1314_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(LDC1314Component)})


@automation.register_action(
    "ldc1314.characterize", CharacterizeAction, LDC1314_ACTION_SCHEMA, synchronous=True
)
async def ldc1314_characterize_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ldc1314.clear_characterization", ClearCharacterizationAction, LDC1314_ACTION_SCHEMA, synchronous=True
)
async def ldc1314_clear_characterization_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ldc1314.clear_overrides", ClearOverridesAction, LDC1314_ACTION_SCHEMA, synchronous=True
)
async def ldc1314_clear_overrides_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ldc1314.set_override",
    SetOverrideAction,
    LDC1314_ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_ARMED): cv.templatable(cv.boolean),
        }
    ),
    synchronous=True,
)
async def ldc1314_set_override_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_ARMED], args, cg.bool_)
    cg.add(var.set_armed(template_))
    return var


@automation.register_condition(
    "ldc1314.is_characterizing", IsCharacterizingCondition, LDC1314_ACTION_SCHEMA
)
async def ldc1314_is_characterizing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
