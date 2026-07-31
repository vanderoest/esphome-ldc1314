# HomeWizard Watermeter — original firmware boot log (cleaned)

Readable reformatting of `docs/original-homewizard-boot.log`, which is stored as a single
unbroken 8.5 KB line with no line terminators. Records have only been split onto separate lines
and grouped under headings. The ESP-IDF `I/W/E (timestamp_ms)` prefixes are preserved so ordering
and timing stay verifiable against the raw file.

Captured from the **stock HomeWizard firmware** (`HWE-WTR`, fw 3.01) on the same hardware this
component now drives. Analysis of the register values lives in
[`homewizard_reference_config.md`](homewizard_reference_config.md); this file is just the log.

> **Redacted.** Identifying values have been replaced with placeholders in both this file and the
> raw `.log` — see "Redactions" at the end. Everything technically relevant to the LDC
> configuration is untouched.

---

## 1. ROM bootloader

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1224
load:0x40078000,len:14276
load:0x40080400,len:3152
entry 0x400805f8
```

Cold boot (power-on reset), fast SPI flash boot, DIO flash mode.

## 2. CPU and application identity

```
I (441) cpu_start: Multicore app
I (442) cpu_start: Pro cpu up.
I (442) cpu_start: Starting app cpu, entry point is 0x400816bc
I (0)   cpu_start: App cpu up.
I (459) cpu_start: Pro cpu start user code
I (460) cpu_start: cpu freq: 240000000 Hz
I (460) cpu_start: Application information:
I (464) cpu_start: Project name: watermeter
I (469) cpu_start: App version: 3.01
I (474) cpu_start: Secure version: 0
I (479) cpu_start: Compile time:
I (483) cpu_start: ELF file SHA256: 8c062cfee9ba9536...
I (489) cpu_start: ESP-IDF: v5.1.2
I (494) cpu_start: Min chip rev: v0.0
I (499) cpu_start: Max chip rev: v3.99
I (503) cpu_start: Chip rev: v1.0
```

Dual-core ESP32 rev 1.0 at 240 MHz, ESP-IDF v5.1.2.

## 3. Heap, flash, core dump

```
I (508) heap_init: Initializing. RAM available for dynamic allocation:
I (515) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (521) heap_init: At 3FFBAFF0 len 00025010 (148 KiB): DRAM
I (528) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (534) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (540) heap_init: At 40098754 len 000078AC (30 KiB): IRAM
I (548) spi_flash: detected chip: generic
I (551) spi_flash: flash io: dio
I (556) esp_core_dump_flash: Init core dump to flash
E (561) esp_core_dump_flash: No core dump partition found!
E (567) esp_core_dump_flash: No core dump partition found!
I (574) app_start: Starting scheduler on CPU0
I (578) app_start: Starting scheduler on CPU1
I (578) main_task: Started on CPU0
I (588) main_task: Calling app_main()
```

## 4. Application startup

```
I (641) appliance: [APP] Running appliance...
I (641) appliance: [APP] Product type: watermeter
I (641) appliance: [APP] Firmware version: 3.01
I (646) appliance: [APP] MAC address: xx:xx:xx:xx:xx:xx (valid)
I (652) appliance: [APP] Free memory: 257312 bytes
I (658) appliance: [APP] Operating mode: normal
I (663) appliance: [APP] Security status: 00 APP_OK_3.01 CLOUD_NVS_OK
W (672) FLASH: Error getting data: (boot_count) (ESP_ERR_NVS_NOT_FOUND)
```

## 5. GPIO configuration

```
E (689) esp_timer: Task is already initialized
I (690) gpio: GPIO[13]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (692) esp_timer: Task is already initialized
E (697) gpio: gpio_install_isr_service(499): GPIO isr service already installed
I (705) gpio: GPIO[32]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (715) esp_timer: Task is already initialized
E (720) gpio: gpio_install_isr_service(499): GPIO isr service already installed
I (728) gpio: GPIO[16]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 1| Intr:3
I (737) gpio: GPIO[16]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 1| Intr:0
I (747) USB_MAIN: USB Boot
I (750) DATABASE: Starting Database task!
I (751) gpio: GPIO[33]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (754) main_task: Returned from app_main()
I (764) gpio: GPIO[35]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (778) gpio: GPIO[19]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 1| Intr:0
```

Observed pin summary — **roles are not stated by the log and are deliberately not guessed here**:

| GPIO | Direction | Pull | Interrupt |
|---|---|---|---|
| 13 | input | pull-up | `Intr:3` |
| 32 | input | pull-up | `Intr:3` |
| 16 | input | pull-down | `Intr:3`, then reconfigured to `Intr:0` |
| 33 | output | pull-up | none |
| 35 | input | pull-up | none |
| 19 | output | pull-down | none |

Note the I²C pins are **not** among these — this component talks to the LDC1314 on GPIO26 (SDA)
and GPIO25 (SCL), verified working in `tests/watermeter-test.log`. Whatever these six pins do
(buttons, LEDs, USB detect, INTB, SD, …) is not evidenced by this log.

## 6. LDC13xx channel initialisation ← the interesting part

```
I (799) LDC13xx: CH[0]| RCOUNT: 0x4d6| OFFSET: 0x3000| SETTLECOUNT: 0xa| CLOCK_DIVIDER: 0x1002| DRIVE_CURRENT: 0xb800
I (803) LDC13xx: CH[1]| RCOUNT: 0x4d6| OFFSET: 0x3000| SETTLECOUNT: 0xa| CLOCK_DIVIDER: 0x1002| DRIVE_CURRENT: 0xb800
I (814) LDC13xx: CH[2]| RCOUNT: 0x4d6| OFFSET: 0x3000| SETTLECOUNT: 0xa| CLOCK_DIVIDER: 0x1002| DRIVE_CURRENT: 0xb800
```

Three channels only (CH0–CH2), all configured identically. Note the log prints **per-channel**
registers only — `CONFIG`, `MUX_CONFIG`, `ERROR_CONFIG` and `RESET_DEV` (which holds
`OUTPUT_GAIN`) are never shown, so those cannot be compared. Decoding and comparison against our
driver: [`homewizard_reference_config.md`](homewizard_reference_config.md).

## 7. NVS / persisted state

```
E (826) FLASH: Error opening NVS handle: (ESP_ERR_NVS_NOT_FOUND)
W (830) FLASH: Error getting data: (c_factor) (ESP_ERR_NVS_NOT_FOUND)
I (838) CLOUD_PUBLISHER: publisher task started
W (839) FLASH: Error getting data: (measurement_id) (ESP_ERR_NVS_NOT_FOUND)
W (850) FLASH: Error getting data: (ESP_ERR_NVS_NOT_FOUND)
```

`c_factor` is the meter's calibration factor — application-level state, deliberately outside this
driver's scope (see `design_decisions.md`).

## 8. WiFi stack init

```
I (859) wifi: Hostname:HW-watermeter-XXXXXX
I (862) wifi:wifi driver task: 3ffca818, prio:23, stack:6656, core=0
I (886) wifi:wifi firmware version: 91b9630
I (886) wifi:wifi certification version: v7.0
I (887) wifi:config NVS flash: enabled
I (887) wifi:config nano formating: disabled
I (891) wifi:Init data frame dynamic rx buffer num: 32
I (896) wifi:Init static rx mgmt buffer num: 5
I (900) wifi:Init management short buffer num: 32
I (904) wifi:Init dynamic tx buffer num: 32
I (908) wifi:Init static rx buffer size: 1600
I (912) wifi:Init static rx buffer num: 10
I (916) wifi:Init dynamic rx buffer num: 32
I (921) wifi_init: rx ba win: 6
I (924) wifi_init: tcpip mbox: 32
I (928) wifi_init: udp mbox: 6
I (931) wifi_init: tcp mbox: 6
I (935) wifi_init: tcp tx win: 5744
I (939) wifi_init: tcp rx win: 5744
I (943) wifi_init: tcp mss: 1440
I (947) wifi_init: WiFi IRAM OP enabled
I (952) wifi_init: WiFi RX IRAM OP enabled
I (958) phy_init: phy_version 4780,16b31a7,Sep 22 2023,20:42:16
I (1033) wifi:mode : sta (xx:xx:xx:xx:xx:xx)
I (1034) wifi:enable tsf
I (1036) wifi:Set ps type: 0, coexist: 0
I (1039) wifi: WiFi Connecting
```

## 9. Local HTTP API

```
I (1052) API: Starting HTTP service...
I (1054) API: Registering endpoint: /api/v1/data/?
I (1055) API: Registering endpoint: /api/v1/identify/?
I (1057) API: Registering endpoint: /api/v1/system/?
I (1062) API: Registering endpoint: /api/v1/system/?
```

## 10. WiFi association

```
I (3452) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1
I (3454) wifi:state: init -> auth (b0)
I (3459) wifi:state: auth -> assoc (0)
I (3467) wifi:state: assoc -> run (10)
I (3522) wifi:connected with <SSID>, aid = 8, channel 1, BW20, bssid = yy:yy:yy:yy:yy:yy
I (3523) wifi:security: WPA2-PSK, phy: bgn, rssi: -60
I (3532) wifi:pm start, type: 0
I (3540) wifi:<ba-add>idx:0 (ifx:0, yy:yy:yy:yy:yy:yy), tid:0, ssn:0, winSize:64
I (3583) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (4536) wifi: WiFi Connected
I (4536) esp_netif_handlers: sta ip: 192.168.x.x, mask: 255.255.255.0, gw: 192.168.x.x
```

## 11. Cloud (MQTT) connection

```
I (4537) cloud: Initializing cloud connection
I (4543) cloud: Connecting to MQTT broker at wss://rain.cloud.homewizard.com/mqtt
I (4556) USB_MAIN: CLOUD_CONNECTING event
I (4611) wifi:<ba-add>idx:1 (ifx:0, yy:yy:yy:yy:yy:yy), tid:3, ssn:0, winSize:64
I (5347) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/total_offset_dl/set
I (5351) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/c_factor/set
I (5358) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/send_interval/set
I (5365) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/batch_send_interval_m/set
I (5375) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/measure_interval/set
I (5383) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/support_controls/set
I (5392) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/keep_awake_timer_m/set
I (5401) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/enable_local_api/set
I (5413) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/state/debug_cal_min_difference/set
I (5420) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/$update/set
I (5428) cloud: Subscribing: appliance/watermeter/xxxxxxxxxxxx/$token
I (5537) USB_MAIN: CLOUD_CONNECTED event
I (5539) app_cloud: [OTA] Marking firmware as valid
I (5540) cloud_sub: Handling message to state/send_interval/set: 40
I (5544) app_cloud: New Send Interval [40]
I (5551) APP_PUBLISHER: Preparing cloud message!
E (5555) FLASH: Error opening NVS handle: (ESP_ERR_NVS_NOT_FOUND)
```

The subscribed topics name the stock firmware's tunables: `c_factor`, `measure_interval`,
`send_interval` (set to 40 here), `total_offset_dl`, `debug_cal_min_difference`. All of these are
application/calibration concerns, not LDC register settings.

## 12. Telemetry (steady state)

```
I (4570)  API: JSON:{"wifi_ssid":"<SSID>","wifi_strength":80,"total_liter_m3":X.XXX,"active_liter_lpm":0,"total_liter_offset_m3":0}
I (4581)  API: JSON:{"cloud_enabled":true}
I (4588)  API: JSON:{"product_name":"Watermeter","product_type":"HWE-WTR","serial":"xxxxxxxxxxxx","firmware_version":"3.01","api_version":"v1"}
I (9257)  API: JSON:{"wifi_ssid":"<SSID>","wifi_strength":82,"total_liter_m3":X.XXX,"active_liter_lpm":0,"total_liter_offset_m3":0}
I (9265)  API: JSON:{"cloud_enabled":true}
I (14256) API: JSON:{"wifi_ssid":"<SSID>","wifi_strength":80,"total_liter_m3":X.XXX,"active_liter_lpm":0,"total_liter_offset_m3":0}
I (14263) API: JSON:{"cloud_enabled":true}
I (19257) API: JSON:{"wifi_ssid":"<SSID>","wifi_strength":80,"total_liter_m3":X.XXX,"active_liter_lpm":0,"total_liter_offset_m3":0}
I (19265) API: JSON:{"cloud_enabled":true}
I (24262) API: JSON:{"wifi_ssid":"<SSID>","wifi_strength":78,"total_liter_m3":X.XXX,"active_liter_lpm":0,"total_liter_offset_m3":0}
I (24271) API: JSON:{"cloud_enabled":true}
I (29256) API: JSON:{"wifi_ssid":"<SSID>","wifi_strength":78,"total_liter_m3":X.XXX,"active_liter_lpm":0,"total_liter_offset_m3":0}
```

Telemetry every ~5 s. `total_liter_m3` is static at X.XXX and `active_liter_lpm` is 0
throughout the capture, i.e. **no flow during this boot log** — so it says nothing about how the
firmware behaves while water is running.

---

## Redactions

Network-, device- and consumption-identifying values were replaced with placeholders in **both**
this file and the raw `docs/original-homewizard-boot.log`, since none of them are needed to
understand the LDC configuration (only §6 matters for that):

| Redacted | Placeholder |
|---|---|
| WiFi SSID | `<SSID>` |
| Router BSSID | `yy:yy:yy:yy:yy:yy` |
| Device MAC | `xx:xx:xx:xx:xx:xx` |
| Device serial (MAC without separators; the cloud MQTT topic key) | `xxxxxxxxxxxx` |
| Derived hostname | `HW-watermeter-XXXXXX` |
| Local IP and gateway | `192.168.x.x` |
| Meter reading (`total_liter_m3`) | `X.XXX` |

Everything else — timestamps, register values, GPIO configuration, firmware versions, heap
layout, the MQTT topic *structure* — is unmodified. The same placeholders were applied to
`tests/watermeter-test.log`. Neither log was ever committed before redaction, so no unredacted
copy exists in the git history.
