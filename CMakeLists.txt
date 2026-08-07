# TemporalAnomalyClock — ESP-IDF top-level project file.
#
# Expects ESP-IDF >= 5.1.
# Build:
#   idf.py set-target esp32c3
#   idf.py menuconfig     (set WiFi, partition table, etc. under "TemporalAnomalyClock")
#   idf.py build flash monitor

cmake_minimum_required(VERSION 3.16)

# Point IDF at our extra components directory. The second entry is the
# GC9A01 driver's nested opt-in component (LVGL-based, kept separate from
# the main esp32_ntp_clock_drivers component) -- see that repo's README.
# Must point directly AT the gc9a01/ subdirectory, not its parent: an
# EXTRA_COMPONENT_DIRS entry that itself has a CMakeLists.txt (like
# esp32_ntp_clock_drivers does) is registered as ONE component and never
# scanned for nested component subdirectories.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components"
    "${CMAKE_SOURCE_DIR}/components/esp32_ntp_clock_drivers/gc9a01"
)

# esp32_ntp_clock_drivers itself (the submodule root, holding the
# non-LVGL drivers we don't use here: MAX6921/PT6315/HT16K33/etc.) gets
# auto-discovered as a subdirectory of the "components" container above,
# purely as a side effect of it needing to be checked out there to reach
# the nested gc9a01/ we actually want. Its own CMakeLists.txt
# unconditionally REQUIRES component names specific to its other
# consumers (e.g. lgl_vfd_studio, MoodWhisperer-only) that don't exist in
# this build, so it must be excluded rather than just left unreferenced.
set(EXCLUDE_COMPONENTS "esp32_ntp_clock_drivers")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TemporalAnomalyClock)
