#include <Arduino.h>
// #include "config.h" // Uncomment once config.h is created from config.h.example

void setup() {
    Serial.begin(115200);
    delay(1000); // Give serial monitor time to connect
    Serial.println("Starting goTapFirmware on ESP32-S3...");

    // Setup WiFi
    // Setup Display / LVGL
    // Fetch initial data
}

void loop() {
    // LVGL tick handler
    // lv_timer_handler();
    // delay(5);

    // Other non-blocking tasks
}
