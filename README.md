# Temporal Anomaly Clock

An ESP-IDF firmware for a round GC9A01 LCD turned NTP clock with a
twist: the displayed time doesn't just tell you the time, it visibly
speeds up, slows down, and occasionally runs backwards — while staying
exactly correct on average.

This is the IDF port of the Arduino-generation `temporal_anomaly_clock`
(originally a MAX6921-driven VFD tube clock), rebuilt on top of
[ESP32NTPClock2](https://github.com/v01dma1n/ESP32NTPClock2) and
[ESP32WiFi2](https://github.com/v01dma1n/ESP32WiFi2), aligned with the
architecture used by [MoodWhisperer](https://github.com/v01dma1n/MoodWhisperer).
Class names, extension points, and file layout mirror that project, so
switching between the two should feel familiar. The VFD tube is gone —
this generation targets a round pixel LCD instead — but the WiFi/NTP/
preferences engine underneath is the same one.

---

## Hardware

| Part                | Role                                                    |
|---------------------|----------------------------------------------------------|
| ESP32-C3-MINI-1U    | MCU (JCZN "ESP32-2424S012"-style integrated board)       |
| GC9A01              | 1.28", 240×240 round IPS LCD, SPI                         |

Pinout (confirmed working on this board — no dedicated RST line, no
built-in touch used):

| Signal      | GPIO |
|-------------|------|
| SPI SCLK    | 6    |
| SPI MOSI    | 7    |
| SPI CS      | 10   |
| SPI DC      | 2    |
| Backlight   | 3    |
| RST         | not connected — software reset only |

The panel needs a horizontal mirror at the LVGL display-config level
(`disp_cfg.rotation.mirror_x = true` in `disp_driver_gc9a01_round_clock.cpp`)
to render right-reading. This must be set in the `lvgl_port_display_cfg_t`
struct itself — a separate `esp_lcd_panel_mirror()` call made before
`lvgl_port_add_disp()` gets silently overwritten, since that function
re-applies orientation from `disp_cfg.rotation` internally.

---

## Project layout

```
temporal_anomaly_clock/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions/
│   └── partitions.csv
├── components/
│   ├── esp32_ntp_clock/    # git submodule → ESP32NTPClock2 (the "engine")
│   └── esp32_wifi/         # git submodule → ESP32WiFi2 (WiFi + prefs + portal)
└── main/                   # the application
    ├── main.cpp
    ├── temporal_anomaly_app.{h,cpp}
    ├── temporal_anomaly_preferences.{h,cpp}
    ├── temporal_anomaly_access_point_manager.{h,cpp}
    ├── disp_driver_gc9a01_round_clock.{h,cpp}
    └── version.h
```

No `esp32_ntp_clock_drivers` submodule — that repo supplies MAX6921/
PT6315-style multiplexed VFD drivers, not applicable to a pixel LCD. The
display driver here is bespoke, living directly in `main/`.

---

## Architecture

### 1. Engine (`components/esp32_ntp_clock`, `components/esp32_wifi`)

Same engine as MoodWhisperer: `BaseNtpClockApp`, `ClockFsmManager`,
`SceneManager`, `IDisplayDriver`, `BasePreferences`,
`BaseAccessPointManager`. See either project's submodule source for the
full interface surface — nothing about it is modified here.

### 2. Display driver (`main/disp_driver_gc9a01_round_clock.{h,cpp}`)

`DispDriverGc9a01RoundClock` has two jobs on one physical screen:

- **`IDisplayDriver` implementation** — satisfies the engine's
  character-grid contract (`setChar`, segment masks, frame buffers) so
  the boot/WiFi/NTP/AP-mode status messages the engine already knows how
  to generate keep working unmodified. There's no real segment glass
  behind it: the plain-text path just pushes to an LVGL status label.
  Segment-specific methods are accepted but no-op — nothing in this app
  drives segment-style scene animations (the `SceneManager` playlist is
  empty).
- **The watch face** — an analog clock (hour/minute/second hands as
  `lv_line` widgets, tight-bounding-box sized per update rather than
  full-screen, to keep SPI traffic and redraw regions small) plus a
  six-slot digital readout (one `lv_label` per digit, since proportional
  font glyphs aren't equal-width and a single shared label visibly
  drifts sideways as digits change).

The two live on separate LVGL screens (`_bootScreen` / `_faceScreen`),
switched via `lv_scr_load()`. `showClockFace()` flips between them once
the FSM reaches `RUNNING_NORMAL`.

### 3. Application (`main/temporal_anomaly_*`)

- `TemporalAnomalyPreferences : BasePreferences` — adds startup-animation
  toggle, OpenWeatherMap key/city, and the anomaly wobble period/amplitude
  on top of `BaseConfig`.
- `TemporalAnomalyAccessPointManager : BaseAccessPointManager` — adds the
  matching rows to the captive portal.
- `TemporalAnomalyClockApp : BaseNtpClockApp` — owns the display driver,
  the display manager, and the shared `WeatherManager`. No scene
  playlist; `loop()` toggles `showClockFace()` directly off FSM state and
  feeds fresh weather readings into the display driver each tick.

---

## The temporal anomaly

The watch face's *displayed* time fluctuates — speeding up, slowing
down, and occasionally running backwards — while staying exactly
correct on average. Modeled as a sinusoidal rate around real time:

```
rate(t) = 1 + A·sin(2π·t/P)
```

Integrating that rate gives the displayed-time offset from real time,
which is periodic with period `P`. Whenever `P` evenly divides 60, the
offset at `t+60` exactly equals the offset at `t` — meaning displayed
time gains exactly as much as it loses over *any* 60-second window, not
just ones aligned to a particular phase. This holds regardless of
amplitude: with `A > 1` the rate swings negative for part of each cycle
and the hands visibly reverse before catching back up.

Configurable in the portal:

- **Anomaly wobble period (sec)** — should divide 60 evenly (e.g. 5, 6,
  10, 12, 15, 20, 30, 60) for the exact-average guarantee; not enforced
  by the form.
- **Anomaly wobble amplitude ×100** — 100 = steady real time, >100 lets
  the hands run backwards for part of each cycle. Default 140 (rate
  swings from −0.4x to 2.4x).

Only the watch-face rendering is perturbed — real system time (NTP
sync, logs, preferences, `time()`) is untouched.

---

## Weather

Pulls current temperature (°F) and relative humidity from
[OpenWeatherMap](https://openweathermap.org/) via the shared
`WeatherManager`, and rotates them into the digital readout row every
5 seconds alongside the time (`TIME → TEMPERATURE → HUMIDITY → TIME →
...`). The analog hands keep ticking continuously regardless of what
the readout row is showing.

Configure in the portal:

- **OpenWeatherMap API Key** — free account at openweathermap.org.
- **OWM City** — comma-separated as the OWM API expects, e.g.
  `South Plainfield,NJ,US` or `Warsaw,PL`. A plain space instead of
  commas will fail to geocode (404).

The temperature/humidity rotation stays on TIME only until the first
successful fetch — it never shows stale or zeroed weather.

---

## Captive portal and timezone handling

Same split as MoodWhisperer:

1. First boot / no credentials stored → AP mode → user enters SSID/
   password and picks a timezone from the dropdown (defaults to UTC).
2. After WiFi connects, `ClockFsmManager` tries automatic timezone
   detection via IP geolocation if the stored timezone is blank, but
   the portal always has the final say.

The portal is open (no password) at `http://192.168.4.1/`, with every
DNS query on the AP network redirected there so phones/laptops pop the
captive-portal sheet automatically.

This app doesn't wire up a physical AP-trigger button (unlike
MoodWhisperer's BOOT-button hold) — the only way to force AP mode is
`BootManager`'s built-in double-reset detection: two resets within
~10 seconds. Worth knowing when iterating with `idf.py flash` followed
by a manual reset for testing — back-to-back resets land you in the
portal.

---

## Build

Requirements: ESP-IDF ≥ 5.1, targeting `esp32c3`.

The `components/esp32_ntp_clock` and `components/esp32_wifi` directories
are git submodules. Initialise them after cloning:

```bash
git clone --recurse-submodules <this-repo>
# or, inside an existing clone:
git submodule update --init
```

Then build and flash:

```bash
. $IDF_PATH/export.sh
cd temporal_anomaly_clock
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash
```

`idf.py monitor` needs an interactive TTY; from a non-interactive shell,
read the serial port directly (e.g. via `pyserial`) instead.

First boot: the board comes up in AP mode (no credentials stored).
Connect to the open `temporal-anomaly` network, the captive portal
opens, fill in WiFi + timezone + (optionally) OpenWeatherMap + anomaly
wobble settings, save.

---

## Known issues

- No external RTC on this board — NTP-only timekeeping. Per
  MoodWhisperer's own notes on the same engine: an NTP timeout with no
  RTC fitted can drop the FSM back into AP_MODE even when WiFi is up
  (`clock_fsm_manager.cpp`, `NTP_SYNC` case). Not yet reproduced on this
  project, but worth knowing if the clock ever seems to bounce back to
  setup mode unexpectedly on a flaky network.
- The "anomaly level" concept from the original Arduino app's
  preferences was dead code there (stored but never actually read) — it
  was not ported. The real wobble effect (period/amplitude, above) is a
  new implementation, not a revival of that setting.

---

## License

MIT (matches the upstream Arduino projects and MoodWhisperer).
