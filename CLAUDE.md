# TemporalAnomalyClock — agent instructions

## Project
ESP-IDF firmware for a round GC9A01 240x240 LCD driving an analog +
digital watch face with a deliberately "wobbly" time source (sinusoidal
drift + damped-random-walk chaos), weather readout, and a photo-mode
rotation with CRT-style scan/noise effects. See README.md for
architecture and `main/temporal_anomaly_app.h`'s header comment for how
it bridges the engine's character-grid boot/status contract onto an
LVGL label.

## Hardware
- ESP32-C3-MINI-1U + 1.28" round GC9A01 LCD, "ESP32-2424S012"-style
  clone board (JCZN). Pin map: SCLK=6, MOSI=7, CS=10, DC=2, no dedicated
  RST, BL=3 (see `disp_driver_gc9a01_round_clock.cpp`'s `#define`s).
- Serial port on the host: /dev/ttyACM0 (adjust if different).
- **No dedicated RST line.** This board's GC9A01 has been observed to
  latch a wrong mirror/orientation bit on some true cold power-ons
  despite identical firmware and no code change -- confirmed via 4
  consecutive real power-cycles after a mirror_x flip, not flaky. The
  current `mirror_x` setting in `begin()` is empirically the stable one
  for the board attached 2026-08-21; if a *replacement* board (not just
  a power-cycle of the same one) shows mirrored, flip the flag and
  re-verify with several genuine unplug/replug cycles -- an `idf.py
  flash` reset does NOT power-cycle the panel and is not a valid test.

## Build / flash loop

Always run from repo root. IDF is already sourced in the user's shell.

    idf.py build
    idf.py -p /dev/ttyACM0 flash

`idf.py monitor` needs an interactive TTY, not available from an agent
shell here. MoodWhisperer's `tools/serial_capture.py` (pyserial,
DTR/RTS reset toggle, `--no-reset` to eavesdrop without re-triggering
boot) is the pattern to copy over if a boot log capture is ever needed.

Stale clangd/IDE diagnostics (unknown compiler flags, `cstdint`/`ctime`
not found, `sinf`/`cosf`/`M_PI` undeclared, `std::make_unique` missing,
etc.) appear after nearly every edit — this is misconfigured IDE
tooling, not real build errors. The actual `idf.py build` (ninja) is
authoritative; verified repeatedly this session that it succeeds
regardless of what clangd reports.

## Firmware versioning

`main/version.h` used to hardcode `VER_MAJOR`/`VER_MINOR`/`VER_BUILD`,
never once bumped since the project's start -- a number that quietly
lied about what was actually flashed. That's gone. Versioning now rides
on ESP-IDF's own git-describe stamp:

- `sdkconfig` leaves `CONFIG_APP_PROJECT_VER_FROM_CONFIG` unset (the
  default), so every build embeds `git describe --always --dirty` (or
  equivalent) as the app version automatically -- no file to edit, no
  step to remember.
- At runtime, `esp_app_get_description()->version` returns that string.
  It's fed to the display driver via
  `DispDriverGc9a01RoundClock::setVersionTag()`, which renders it as a
  small permanent footer on the boot screen (visible through the whole
  boot sequence — WiFi connect, NTP sync, AP mode — not just the
  startup splash).
- **Tag meaningful milestones**: `git tag v1.1.0` (annotated or
  lightweight, either works) at a commit makes that commit's build show
  a clean `v1.1.0`. Commits after the tag show `v1.1.0-3-g1a2b3c4`
  (3 commits past the tag, at that hash); a dirty working tree appends
  `-dirty`. Untagged, it just falls back to the bare hash — still
  accurate, just less pretty.
- Push tags explicitly: `git push origin main` does **not** push tags;
  use `git push origin <tag>` (or `--tags`) separately.
- This convention is TemporalAnomalyClock-specific for now (adopted
  2026-08-21). Bring it up before assuming other projects in this
  portfolio use the same scheme — check each project's own CLAUDE.md
  first, don't assume this one's approach transfers silently.

## Conventions
- Base class names (BaseConfig, BaseAccessPointManager, BaseNtpClockApp,
  IBaseClock, IDisplayDriver) mirror ESP32NTPClock on Arduino. Preserve
  them when refactoring.
- The engine is in `components/esp32_ntp_clock/`. The GC9A01 driver is
  in `components/esp32_ntp_clock_drivers/gc9a01/`. The application is in
  `main/`. All three are independent git repos (two submodules + this
  parent) — commit inside each submodule first, then the parent (which
  records the updated submodule pointer SHAs), then push submodules
  before the parent.
- Comments and logs match the existing casual, specific voice. No
  emoji, no marketing words, no "comprehensive solution" phrasing.
- Image assets (dial background, photo-rotation faces) are generated
  via `scripts/convert_photo.py <source> <var_name> [header.h]` from a
  240x240 (or resizable-to-240x240) source into `main/<var_name>_data.cpp`.
  The generated header comment defaults to crediting Google Gemini —
  correct it by hand when a source is code-synthesized or from
  elsewhere, since it resets on every regeneration.

## Known issues / in flight
- Cold-boot mirror flakiness on RST-less GC9A01 boards — see Hardware
  section above. Root cause (why a true POR differs from a warm EN
  reset for this one register) is unconfirmed; a pre-init settle delay
  and a late redundant `esp_lcd_panel_mirror()` re-issue were both
  tried and neither changed the behavior on the board that showed it.
- A previous physical board (before 2026-08-21) showed a naked-eye
  "vertical stripes" display artifact, isolated via extensive bisection
  to be unrelated to any tested firmware factor — suspected loose
  connector/hardware fault on that specific unit, not this codebase.
  Paused, not resolved; don't resume investigating it unless the user
  raises it again (that board may no longer even be in use).

## Do not
- Don't propose speculative "maybe this helps" timing/register changes
  to the display driver without a plan to verify on real hardware
  across multiple genuine power-cycles, not just `idf.py flash` resets
  — the mirror bug above was chased for a while specifically because a
  warm reset looked like a valid test and wasn't.
- Don't touch `.git/` or rewrite history.
- Don't run `idf.py fullclean` unless a build is genuinely broken —
  full rebuilds cost 2+ minutes.
