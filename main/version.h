#pragma once

// APP_DATE is the design/branding year shown on the dial watermark and
// startup splash -- not a build version, doesn't need to track commits.
#define APP_NAME   "Temporal Anomaly Clock"
#define APP_AUTHOR "v01dma1n"
#define APP_DATE   "2026-08-06"

// Firmware version is NOT hand-maintained here (a VER_MAJOR/MINOR/BUILD
// trio existed previously and was never once bumped -- dead weight that
// silently lied). ESP-IDF already stamps every build with a git-describe
// string (see sdkconfig: CONFIG_APP_PROJECT_VER_FROM_CONFIG is not set,
// the default), retrievable at runtime via
// esp_app_get_description()->version (see temporal_anomaly_app.cpp's
// startup splash). Tag meaningful milestones (`git tag v1.1.0`) so that
// string reads as a clean version instead of a bare commit hash --
// untagged, it still shows *something* accurate, just less pretty.
