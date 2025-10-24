#include "temporal_anomaly_app.h"
#include "vfd_hardware_map.h"
#include <ESP32NTPClock.h>
#include "ESP32NTPClock_MAX6921_Hardware.h" // Must include the hardware driver

// --- Task Configuration ---
#define SERIAL_TIMEOUT_MS 2000
#define STARTUP_DELAY_MS 300

#define DISPLAY_TASK_STACK 4096
#define DISPLAY_TASK_PRIO 10
#define DISPLAY_TASK_CORE 1

#define ANIM_TASK_STACK 16384
#define ANIM_TASK_PRIO 5
#define ANIM_TASK_CORE 0

SemaphoreHandle_t serialMutex = NULL;
AppLogLevel g_appLogLevel = APP_LOG_INFO;
TemporalAnomalyClockApp& app = TemporalAnomalyClockApp::getInstance();

/**
 * @brief High-priority task to handle display multiplexing (CONSUMER).
 * Runs on Core 1 (Real-time).
 */
void displayTask(void *pvParameters) {

  DispDriverMAX6921_Hardware hardwareDriver(
      VSPI_SCLK, 
      VSPI_MISO, 
      VSPI_MOSI, 
      VSPI_SS, 
      VSPI_BLANK,
      VFD_GRIDS, 
      VFD_DIGIT_COUNT
  );
  hardwareDriver.begin();

  IDisplayDriver& logicalDisplay = app.getDisplay();
  int displaySize = logicalDisplay.getDisplaySize();
  // Dynamically allocate a buffer based on the actual display size
  unsigned long* localFrame = new unsigned long[displaySize]();
  
  int currentDigit = 0;
  for (;;) {

    logicalDisplay.getFrameData(localFrame);

    // Write the current digit from our local buffer to the display
    hardwareDriver.writeDigit(currentDigit, localFrame[currentDigit]);

    // Move to the next digit
    currentDigit = (currentDigit + 1) % VFD_DIGIT_COUNT;

    // 1ms delay for a 100Hz refresh rate (1ms * 10 digits)
    vTaskDelay(pdMS_TO_TICKS(1));
    // delayMicroseconds(500);
  }
}

/**
 * @brief Application logic task (PRODUCER).
 * Runs on Core 0 (Network/Logic).
 */
void animationTask(void *pvParameters) {
  // 1. Run the application setup ONCE.
  app.setup();

  // 2. Run the application loop forever.
  for (;;) {
    app.loop();

    // Yield the CPU. The app.loop() has its own timing.
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  delay(300);
  Serial.begin(115200);
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 2000)) {
    ; // Wait for serial port to connect
  }
  Serial.println("\n>>> Starting Temporal Anomaly Clock...");
  
  serialMutex = xSemaphoreCreateMutex();

  // Create the high-priority display task on CORE 1
  xTaskCreatePinnedToCore(
      displayTask,        // Task function
      "DisplayTask",      // Name for debugging
      DISPLAY_TASK_STACK, // Stack size
      NULL,               // Task input parameter
      DISPLAY_TASK_PRIO,  // Priority (high)
      NULL,               // Task handle
      DISPLAY_TASK_CORE); // Core ID

  // Create the lower-priority application task on CORE 0
  xTaskCreatePinnedToCore(
      animationTask,    // Task function
      "AnimationTask",  // Name for debugging
      ANIM_TASK_STACK,  // Stack size (Increased for WiFi/app logic)
      NULL,             // Task input parameter
      ANIM_TASK_PRIO,   // Priority (normal)
      NULL,             // Task handle
      ANIM_TASK_CORE);  // Core ID

  Serial.println("\n>>> ... setup complete, tasks running");
}

void loop() {
  // The main Arduino loop (running on Core 1) is no longer used.
  // Suspend this task to free up its resources.
  vTaskDelay(portMAX_DELAY);
}