#ifndef VERSION_H
#define VERSION_H

#define APP_NAME "Temporal Anomaly Clock"
#define APP_AUTHOR "v01dma1n"
#define APP_DATE "2025-10-13"

#define VER_MAJOR 0
#define VER_MINOR 1
#define VER_BUILD 0

#define APP_MESSAGE "The clock connects to WiFi. Double-press Reset for Access Point."

#include <string>

#define VERSION_STRING \
  (std::to_string(VER_MAJOR) + "." + std::to_string(VER_MINOR) + "." + std::to_string(VER_BUILD))

#endif  // VERSION_H
