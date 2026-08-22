import math

from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.helpers import fnv1_hash

CODEOWNERS = ["@vanderoest"]
MULTI_CONF = True

watermeter_ns = cg.esphome_ns.namespace("watermeter")
WatermeterComponent = watermeter_ns.class_("WatermeterComponent", cg.Component)
SetTotalAction = watermeter_ns.class_("SetTotalAction", automation.Action)

ReverseMode = watermeter_ns.enum("ReverseMode")
REVERSE_MODES = {
    "SUBTRACT": ReverseMode.WATERMETER_REVERSE_SUBTRACT,
    "IGNORE": ReverseMode.WATERMETER_REVERSE_IGNORE,
}

CONF_WATERMETER_ID = "watermeter_id"
CONF_PHASES = "phases"
CONF_LITERS_PER_REVOLUTION = "liters_per_revolution"
CONF_DIRECTION = "direction"
CONF_REVERSE = "reverse"
CONF_HYSTERESIS = "hysteresis"
CONF_MIN_SIGNAL = "min_signal"
CONF_SAVE_THRESHOLD = "save_threshold"
CONF_SAVE_INTERVAL = "save_interval"
CONF_INITIAL_VALUE = "initial_value"
CONF_NO_FLOW_TIMEOUT = "no_flow_timeout"

# "cw"/"ccw" don't encode a physically-verified sense here -- rotation sense has to be settled per
# installation against the actual wiring/geometry (design_decisions.md), which is exactly what
# this option is for. It's a binary toggle with a memorable name, not a claim about which
# direction is physically clockwise.
DIRECTIONS = {
    "CW": False,   # False -> DecoderConfig.invert_direction unset
    "CCW": True,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WatermeterComponent),
        cv.Required(CONF_PHASES): cv.All(
            cv.ensure_list(cv.use_id(sensor.Sensor)), cv.Length(min=3, max=3)
        ),
        cv.Optional(CONF_LITERS_PER_REVOLUTION, default=1.0): cv.positive_float,
        cv.Optional(CONF_DIRECTION, default="CW"): cv.enum(DIRECTIONS, upper=True),
        cv.Optional(CONF_REVERSE, default="SUBTRACT"): cv.enum(REVERSE_MODES, upper=True),
        cv.Optional(CONF_HYSTERESIS, default="10deg"): cv.angle,
        cv.Optional(CONF_MIN_SIGNAL, default=0.3): cv.percentage,
        cv.Optional(CONF_SAVE_THRESHOLD, default=1.0): cv.positive_float,
        cv.Optional(CONF_SAVE_INTERVAL, default="60s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_INITIAL_VALUE, default=0.0): cv.float_,
        # How long "flowing"/"continuous_flow"/flow_rate keep reporting activity after the last
        # real increment before deciding flow has stopped. Meter-specific: it needs to be
        # comfortably longer than the gap between hysteresis-confirmed steps at this meter's own
        # Q1 (minimum flow), or a genuine slow leak will never be recognized as continuous. Found
        # in code review: this used to be a short, hardcoded 5s tied to the decoder's own
        # envelope-protection timing, which broke `continuous_flow`'s `delayed_on: 30min` filter
        # at low flow (see watermeter.h's set_no_flow_timeout_s()). 60s default is generous
        # relative to this meter's own ~16.7s inter-step gap at Q3 2.5's Q1 -- tune down for a
        # more responsive `flowing` indicator, or up for a meter with a lower Q1.
        cv.Optional(CONF_NO_FLOW_TIMEOUT, default="60s"): cv.positive_time_period_seconds,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    phases = [await cg.get_variable(phase_id) for phase_id in config[CONF_PHASES]]
    cg.add(var.set_phase_sensors(phases[0], phases[1], phases[2]))

    cg.add(var.set_liters_per_revolution(config[CONF_LITERS_PER_REVOLUTION]))
    cg.add(var.set_invert_direction(config[CONF_DIRECTION]))
    cg.add(var.set_reverse_mode(config[CONF_REVERSE]))
    cg.add(var.set_hysteresis_rad(math.radians(config[CONF_HYSTERESIS])))
    cg.add(var.set_min_signal(config[CONF_MIN_SIGNAL]))
    cg.add(var.set_save_threshold_l(config[CONF_SAVE_THRESHOLD]))
    cg.add(var.set_save_interval_s(config[CONF_SAVE_INTERVAL]))
    cg.add(var.set_initial_value_l(config[CONF_INITIAL_VALUE]))
    cg.add(var.set_no_flow_timeout_s(config[CONF_NO_FLOW_TIMEOUT]))
    # Distinguishes preferences between multiple watermeter: instances on one device -- see
    # watermeter.cpp setup(). Matches esphome's own C++/Python fnv1_hash so the two sides agree.
    cg.add(var.set_name_hash(fnv1_hash(str(config[CONF_ID]))))


SET_TOTAL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(WatermeterComponent),
        cv.Required("value"): cv.templatable(cv.float_),
    }
)


@automation.register_action(
    "watermeter.set_total", SetTotalAction, SET_TOTAL_ACTION_SCHEMA, synchronous=True
)
async def watermeter_set_total_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config["value"], args, cg.double)
    cg.add(var.set_value(template_))
    return var
