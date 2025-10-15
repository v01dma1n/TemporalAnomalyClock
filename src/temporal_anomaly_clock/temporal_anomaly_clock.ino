#include "temporal_anomaly_app.h"
#include <ESP32NTPClock.h>

SemaphoreHandle_t serialMutex = NULL;
AppLogLevel g_appLogLevel = APP_LOG_INFO;
TemporalAnomalyClockApp& app = TemporalAnomalyClockApp::getInstance();

void setup() {
  delay(300); 
  Serial.begin(115200);
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 2000)) {
    ; // Wait for serial port to connect, with a 2-second timeout
  }
  Serial.println("\n>>> Starting Temporal Anomaly Clock...");
  
  serialMutex = xSemaphoreCreateMutex();
  app.setup();

  Serial.println("\n>>> ... setup complete");
}

void loop() {
  app.loop();
}
