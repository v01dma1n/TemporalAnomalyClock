#ifndef VFD_HARDWARE_MAP_H
#define VFD_HARDWARE_MAP_H

#include "seven_segment_map.h" // For the VfdSegmentBitmaskMap struct
#include "Arduino.h"         // For static const

// --- 1. SPI Pin Definitions ---
#define VSPI_SCLK   18
#define VSPI_MISO   19
#define VSPI_MOSI   23
#define VSPI_SS      5
#define VSPI_BLANK   0

// --- 2. Grid (Digit) Pin Mapping ---
static const unsigned long VFD_GRIDS[] = {
    0b00000000010000000000, // GRD_01 (Digit 0)
    0b00000000100000000000, // GRD_02 (Digit 1)
    0b00000010000000000000, // GRD_03 (Digit 2)
    0b00010000000000000000, // GRD_04 (Digit 3)
    0b10000000000000000000, // GRD_05 (Digit 4)
    0b00000000000000000010, // GRD_06 (Digit 5)
    0b00000000000000000100, // GRD_07 (Digit 6)
    0b00000000000000100000, // GRD_08 (Digit 7)
    0b00000000000100000000, // GRD_09 (Digit 8)
    0b00000000001000000000  // GRD_10 (Digit 9)
};
static const int VFD_DIGIT_COUNT = sizeof(VFD_GRIDS) / sizeof(VFD_GRIDS[0]);


// --- 3. Segment Bitmask Mapping ---
static const SevenSegmentBitmaskMap s_vfd_segment_map = {
    .segA   = 0b00000000000010000000, //
    .segB   = 0b00000000000001000000, //
    .segC   = 0b00000000000000010000, //
    .segD   = 0b01000000000000000000, //
    .segE   = 0b00001000000000000000, //
    .segF   = 0b00000100000000000000, //
    .segG   = 0b00000001000000000000, //
    .segDot = 0b00000000000000001000  //
};

#endif // VFD_HARDWARE_MAP_H