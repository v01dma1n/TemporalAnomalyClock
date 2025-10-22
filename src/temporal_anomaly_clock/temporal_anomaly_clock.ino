#include "temporal_anomaly_app.h"
#include <ESP32NTPClock.h>
#include "ESP32NTPClock_MAX6921_Hardware.h" // Must include the hardware driver

SemaphoreHandle_t serialMutex = NULL;
AppLogLevel g_appLogLevel = APP_LOG_INFO;
TemporalAnomalyClockApp& app = TemporalAnomalyClockApp::getInstance();

// --- Define the data structure and queue for communication ---
#define DISPLAY_DIGITS 10
typedef unsigned long DisplayFrame[DISPLAY_DIGITS];

QueueHandle_t frameQueue;

/**
 * @brief High-priority task to handle display multiplexing (CONSUMER).
 * Runs on Core 1 (Real-time).
 */
void displayTask(void *pvParameters) {
  // sclkPin, misoPin, mosiPin, ssPin, blankPin
  DispDriverMAX6921_Hardware hardwareDriver(18, 19, 23, 5, 0);
  hardwareDriver.begin();

  DisplayFrame localFrame;
  memset(localFrame, 0, sizeof(DisplayFrame));
  int currentDigit = 0;

  for (;;) {
    // Check for a new frame (non-blocking)
    if (xQueueReceive(frameQueue, &localFrame, 0) == pdTRUE) {
      // We got a new frame
    }

    // Write the current digit from our local buffer to the display
    hardwareDriver.writeDigit(currentDigit, localFrame[currentDigit]);

    // Move to the next digit
    currentDigit = (currentDigit + 1) % DISPLAY_DIGITS;

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
  
  g_appLogLevel = APP_LOG_DEBUG; // Force debug level for testing

  serialMutex = xSemaphoreCreateMutex();

  // Create the frame queue
  frameQueue = xQueueCreate(
    1,                  // Queue depth of 1 (mailbox)
    sizeof(DisplayFrame) // Size of the new frame structure
  );

  if (frameQueue == NULL) {
    Serial.println("Error creating the frame queue");
    while (1); // Halt
  }

  // Create the high-priority display task on CORE 1
  xTaskCreatePinnedToCore(
      displayTask,      // Task function
      "DisplayTask",    // Name for debugging
      4096,             // Stack size
      NULL,             // Task input parameter
      10,               // Priority (high)
      NULL,             // Task handle
      1);               // Core ID

  // Create the lower-priority application task on CORE 0
  xTaskCreatePinnedToCore(
      animationTask,    // Task function
      "AnimationTask",  // Name for debugging
      16384,            // Stack size (Increased for WiFi/app logic)
      NULL,             // Task input parameter
      5,                // Priority (normal)
      NULL,             // Task handle
      0);               // Core ID

  Serial.println("\n>>> ... setup complete, tasks running");
}

void loop() {
  // The main Arduino loop (running on Core 1) is no longer used.
  // Suspend this task to free up its resources.
  vTaskDelay(portMAX_DELAY);
}