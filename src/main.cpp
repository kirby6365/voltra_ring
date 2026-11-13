/*
 * Voltra Remote - BLE remote for the Beyond Power Voltra I
 * Target: Waveshare ESP32-S3-Touch-LCD-1.47 in the voltra_ring case (scroll wheel + button)
 */
#include <Arduino.h>

#include "hw/battery.h"
#include "hw/button.h"
#include "hw/display.h"
#include "hw/haptics.h"
#include "hw/i2c_bus.h"
#include "hw/knob.h"
#include "hw/power.h"
#include "hw/touch_axs5106l.h"
#include "ui/ui.h"
#include "voltra/voltra_client.h"

void setup()
{
    // Woken from power off: goes straight back to sleep unless the button is held.
    power_boot_check();

    Serial.begin(115200);
    delay(100);
    log_i("Voltra Remote starting (free heap %u)", (unsigned)ESP.getFreeHeap());

    i2c_bus_init();
    touch_init();
    haptics_init();
    battery_init();
    knob_init();
    button_init();

    display_init();

    // The client must come up before the UI: ui_init() reads the current device
    // state, and Client::begin() is what creates the mutex guarding it.
    voltra::Client::instance().begin();

    {
        LvLock lock;
        ui_init();
    }
    log_i("ready (free heap %u)", (unsigned)ESP.getFreeHeap());
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
