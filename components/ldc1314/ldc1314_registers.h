#pragma once

// Register addresses and bit-field constants for the LDC1312/LDC1314, transcribed 1:1 from
// register_map.md (itself derived from the TI datasheet SNOSCZ0A, docs/LDC1314_datasheet.md
// §7.6). Do not add fields here that aren't documented there.
//
// v1 scope is the LDC1314 only (see design_decisions.md "Scope v1 to the LDC1314 only, without
// closing the door on LDC1312/1612/1614") -- this header intentionally only covers the
// LDC1312/1314 register layout. The LDC1612/1614 use a different, undocumented-in-this-repo
// register layout (28-bit data, different literature/SNOSCY9) and would need a separate header,
// not an extension of this one.

#include <cstdint>

namespace esphome {
namespace ldc1314 {

// Channel count supported by the LDC1314. Kept as a single named constant (rather than a
// scattered literal) so a future LDC1312 (2-channel) variant only needs this value changed --
// see design_decisions.md.
static const uint8_t MAX_CHANNELS = 4;

// --- Register addresses -----------------------------------------------------------------

static const uint8_t REG_DATA0 = 0x00;
static const uint8_t REG_RCOUNT0 = 0x08;
static const uint8_t REG_OFFSET0 = 0x0C;
static const uint8_t REG_SETTLECOUNT0 = 0x10;
static const uint8_t REG_CLOCK_DIVIDERS0 = 0x14;
static const uint8_t REG_STATUS = 0x18;
static const uint8_t REG_ERROR_CONFIG = 0x19;
static const uint8_t REG_CONFIG = 0x1A;
static const uint8_t REG_MUX_CONFIG = 0x1B;
static const uint8_t REG_RESET_DEV = 0x1C;
static const uint8_t REG_DRIVE_CURRENT0 = 0x1E;
static const uint8_t REG_MANUFACTURER_ID = 0x7E;
static const uint8_t REG_DEVICE_ID = 0x7F;

// Per-channel registers are spaced by a regular offset from their channel-0 address: DATAx by 2,
// everything else by 1 (register_map.md).
inline uint8_t data_register(uint8_t channel) { return REG_DATA0 + channel * 2; }
inline uint8_t rcount_register(uint8_t channel) { return REG_RCOUNT0 + channel; }
inline uint8_t offset_register(uint8_t channel) { return REG_OFFSET0 + channel; }
inline uint8_t settlecount_register(uint8_t channel) { return REG_SETTLECOUNT0 + channel; }
inline uint8_t clock_dividers_register(uint8_t channel) { return REG_CLOCK_DIVIDERS0 + channel; }
inline uint8_t drive_current_register(uint8_t channel) { return REG_DRIVE_CURRENT0 + channel; }

// --- Identification ---------------------------------------------------------------------

static const uint16_t MANUFACTURER_ID_VALUE = 0x5449;
static const uint16_t DEVICE_ID_VALUE = 0x3054;

// --- DATAx (per channel) ------------------------------------------------------------------

static const uint16_t DATA_RESULT_MASK = 0x0FFF;  // bits 11:0
static const uint16_t DATA_ERR_UR = 1 << 15;
static const uint16_t DATA_ERR_OR = 1 << 14;
static const uint16_t DATA_ERR_WD = 1 << 13;
static const uint16_t DATA_ERR_AE = 1 << 12;

// --- CONFIG (0x1A) -------------------------------------------------------------------------

static const uint16_t CONFIG_ACTIVE_CHAN_SHIFT = 14;
static const uint16_t CONFIG_SLEEP_MODE_EN = 1 << 13;
static const uint16_t CONFIG_RP_OVERRIDE_EN = 1 << 12;
static const uint16_t CONFIG_SENSOR_ACTIVATE_SEL = 1 << 11;
static const uint16_t CONFIG_AUTO_AMP_DIS = 1 << 10;
static const uint16_t CONFIG_REF_CLK_SRC = 1 << 9;
static const uint16_t CONFIG_INTB_DIS = 1 << 7;
static const uint16_t CONFIG_HIGH_CURRENT_DRV = 1 << 6;
// Bits 5:0 are reserved and must be written as 0b00'0001 (datasheet §7.6.24).
static const uint16_t CONFIG_RESERVED_BITS = 0x0001;

// --- MUX_CONFIG (0x1B) ---------------------------------------------------------------------

static const uint16_t MUX_CONFIG_AUTOSCAN_EN = 1 << 15;
static const uint16_t MUX_CONFIG_RR_SEQUENCE_SHIFT = 13;
// Bits 12:3 are reserved and must be written as 00'0100'0001 (datasheet §7.6.25).
static const uint16_t MUX_CONFIG_RESERVED_BITS = 0x0208;
static const uint16_t MUX_CONFIG_DEGLITCH_MASK = 0x0007;

// --- ERROR_CONFIG (0x19) -------------------------------------------------------------------

static const uint16_t ERROR_CONFIG_UR_ERR2OUT = 1 << 15;
static const uint16_t ERROR_CONFIG_OR_ERR2OUT = 1 << 14;
static const uint16_t ERROR_CONFIG_WD_ERR2OUT = 1 << 13;
static const uint16_t ERROR_CONFIG_AH_ERR2OUT = 1 << 12;
static const uint16_t ERROR_CONFIG_AL_ERR2OUT = 1 << 11;
static const uint16_t ERROR_CONFIG_UR_ERR2INT = 1 << 7;
static const uint16_t ERROR_CONFIG_OR_ERR2INT = 1 << 6;
static const uint16_t ERROR_CONFIG_WD_ERR2INT = 1 << 5;
static const uint16_t ERROR_CONFIG_AH_ERR2INT = 1 << 4;
static const uint16_t ERROR_CONFIG_AL_ERR2INT = 1 << 3;
static const uint16_t ERROR_CONFIG_ZC_ERR2INT = 1 << 2;
static const uint16_t ERROR_CONFIG_DRDY_2INT = 1 << 0;

// --- STATUS (0x18) -------------------------------------------------------------------------

static const uint16_t STATUS_ERR_CHAN_SHIFT = 14;
static const uint16_t STATUS_ERR_CHAN_MASK = 0x0003;
static const uint16_t STATUS_ERR_UR = 1 << 13;
static const uint16_t STATUS_ERR_OR = 1 << 12;
static const uint16_t STATUS_ERR_WD = 1 << 11;
static const uint16_t STATUS_ERR_AHE = 1 << 10;
static const uint16_t STATUS_ERR_ALE = 1 << 9;
static const uint16_t STATUS_ERR_ZC = 1 << 8;
static const uint16_t STATUS_DRDY = 1 << 6;

// STATUS.UNREADCONVx bits are in *descending* channel order: bit 3 = channel 0, bit 0 = channel 3
// (register_map.md / datasheet §7.6.22) -- not the same order as the channel index itself.
inline uint16_t status_unreadconv_bit(uint8_t channel) { return 1 << (3 - channel); }

// --- RESET_DEV (0x1C) ----------------------------------------------------------------------

static const uint16_t RESET_DEV_RESET = 1 << 15;
static const uint16_t RESET_DEV_OUTPUT_GAIN_SHIFT = 9;
static const uint16_t RESET_DEV_OUTPUT_GAIN_MASK = 0x0003;

// --- CLOCK_DIVIDERSx (per channel) ----------------------------------------------------------

static const uint16_t CLOCK_DIVIDERS_FIN_DIVIDER_SHIFT = 12;
static const uint16_t CLOCK_DIVIDERS_FREF_DIVIDER_MASK = 0x03FF;  // bits 9:0

// --- DRIVE_CURRENTx (per channel) -----------------------------------------------------------

static const uint16_t DRIVE_CURRENT_IDRIVE_SHIFT = 11;

// INIT_IDRIVEx (bits 10:6) is read-only: the drive current the device's own auto-amplitude
// calibration settled on for this channel. Only meaningful while CONFIG.RP_OVERRIDE_EN=0 (auto
// mode); the characterization engine reads it, nothing writes it. See register_map.md and
// datasheet §7.6.27/§8.1.5.2.
static const uint16_t DRIVE_CURRENT_INIT_IDRIVE_SHIFT = 6;
static const uint16_t DRIVE_CURRENT_INIT_IDRIVE_MASK = 0x1F;  // 5 bits

}  // namespace ldc1314
}  // namespace esphome
