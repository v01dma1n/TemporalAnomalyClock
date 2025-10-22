#ifndef DEBUG_H
#define DEBUG_H

#include "Arduino.h"
#include "temporal_anomaly_types.h" 

extern SemaphoreHandle_t serialMutex;
extern AppLogLevel g_appLogLevel;

/*
#define LOGMSG(level, format, ...) \
    do { \
        if (g_appLogLevel >= level) { \
            if (serialMutex != NULL && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) { \
                Serial.printf(format "\n", ##__VA_ARGS__); \
                xSemaphoreGive(serialMutex); \
            } \
        } \
    } while (0)
*/
#define LOGMSG(level, format, ...) \
    do { \
        /* TEMPORARY: Bypass level check and mutex */ \
        Serial.printf(format "\n", ##__VA_ARGS__); \
    } while (0)

#define LOGERR(format, ...) LOGMSG(APP_LOG_ERROR, format, ##__VA_ARGS__)
#define LOGINF(format, ...) LOGMSG(APP_LOG_INFO,  format, ##__VA_ARGS__)
#define LOGDBG(format, ...) LOGMSG(APP_LOG_DEBUG, format, ##__VA_ARGS__)

#endif // DEBUG_H
