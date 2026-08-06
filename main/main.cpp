// main.cpp — ESP-IDF entry point for temporal_anomaly_clock.
//
// Task layout (single-core ESP32-C3, so no PinnedToCore split like
// MoodWhisperer's dual-core ESP32-WROOM):
//   AppTask     — setup() + loop(): WiFi, NTP, portal, FSM.
//   DisplayTask — pumps DisplayManager::update() at ~5 Hz so the
//                 boot/status text (WiFi connect, NTP sync, AP mode)
//                 keeps refreshing even while AppTask is blocked inside
//                 activateAccessPoint()'s runBlockingLoop(). The watch
//                 face itself doesn't need this: it's driven by its own
//                 LVGL timer inside DispDriverGc9a01RoundClock once
//                 shown, same as MoodWhisperer's VFD needed a much
//                 faster DisplayTask (50 Hz) to multiplex digits — our
//                 display isn't multiplexed, so 5 Hz is plenty for text.

#include "temporal_anomaly_app.h"
#include "logging.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr int APP_TASK_STACK = 8192;
static constexpr int APP_TASK_PRIO  = 5;

static constexpr int DISPLAY_TASK_STACK = 4096;
static constexpr int DISPLAY_TASK_PRIO  = 4;

static void displayTask(void* /*pvParameters*/) {
    TemporalAnomalyClockApp& app = TemporalAnomalyClockApp::getInstance();
    DisplayManager& dm = app.getClock();
    for (;;) {
        dm.update();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void appTask(void* /*pvParameters*/) {
    auto& app = TemporalAnomalyClockApp::getInstance();
    app.setup();

    xTaskCreate(displayTask, "DisplayTask", DISPLAY_TASK_STACK, nullptr,
                DISPLAY_TASK_PRIO, nullptr);

    for (;;) {
        app.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void) {
    LOGINF(">>> temporal_anomaly_clock booting");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    xTaskCreate(appTask, "AppTask", APP_TASK_STACK, nullptr, APP_TASK_PRIO, nullptr);

    LOGINF(">>> tasks running; app_main returning to idle");
}
