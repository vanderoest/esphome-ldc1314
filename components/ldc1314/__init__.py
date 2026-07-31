import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Placeholder maintainer handle -- update if/when this component is proposed for esphome/esphome
# core.
CODEOWNERS = ["@vanderoest"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

ldc1314_ns = cg.esphome_ns.namespace("ldc1314")
LDC1314Component = ldc1314_ns.class_("LDC1314Component", cg.PollingComponent, i2c.I2CDevice)

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
