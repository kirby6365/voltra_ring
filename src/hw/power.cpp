#include "power.h"

#include <Arduino.h>

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

#include "board_pins.h"
#include "display.h"
#include "i2c_bus.h"
#include "touch_axs5106l.h"

namespace {
constexpr uint32_t RELEASE_DEBOUNCE_MS = 50;

/** Drive a pin and latch it there through deep sleep. */
void hold_level(int pin, int level)
{
    const gpio_num_t p = (gpio_num_t)pin;
    gpio_hold_dis(p);
    gpio_reset_pin(p);   // detach any peripheral (the backlight's LEDC) from the pad
    gpio_set_direction(p, GPIO_MODE_OUTPUT);
    gpio_set_level(p, level);
    gpio_hold_en(p);
}

void release_holds()
{
    gpio_hold_dis((gpio_num_t)PIN_LCD_BL);
    gpio_hold_dis((gpio_num_t)PIN_LCD_RST);
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_RST);
#if SOC_GPIO_SUPPORT_HOLD_IO_IN_DSLP && !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    gpio_deep_sleep_hold_dis();
#endif
    const int rtc_pins[] = {PIN_ENC_A, PIN_ENC_B, PIN_BUTTON};
    for (int pin : rtc_pins) {
        rtc_gpio_hold_dis((gpio_num_t)pin);
        rtc_gpio_deinit((gpio_num_t)pin);   // back to the digital pad
    }
}

/** Wait until the button has been up for RELEASE_DEBOUNCE_MS. */
void wait_for_release()
{
    uint32_t up_since = 0;
    for (;;) {
        if (digitalRead(PIN_BUTTON) == HIGH) {
            if (!up_since) up_since = millis();
            else if (millis() - up_since >= RELEASE_DEBOUNCE_MS) return;
        } else {
            up_since = 0;
        }
        delay(5);
    }
}

[[noreturn]] void sleep_now()
{
    // Backlight off; the touch controller and the panel held out of reset so they stay in
    // the sleep modes they were sent to.
    hold_level(PIN_LCD_BL, 0);
    hold_level(PIN_TOUCH_RST, 1);
    hold_level(PIN_LCD_RST, 1);
    gpio_pullup_dis((gpio_num_t)PIN_TOUCH_INT);
#if SOC_GPIO_SUPPORT_HOLD_IO_IN_DSLP && !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    gpio_deep_sleep_hold_en();
#endif
    // The encoder pins are RTC pads, whose pull-ups would stay on through deep sleep and
    // draw current through whichever contact the wheel rests on. Cut them off entirely.
    rtc_gpio_isolate((gpio_num_t)PIN_ENC_A);
    rtc_gpio_isolate((gpio_num_t)PIN_ENC_B);

    // Wake on the button alone: pulled up by the RTC domain, waking when pressed to GND.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON, 0);
    rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTON);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTON);
    esp_deep_sleep_start();
}
/**
 * A wake press that was let go too soon. The touch controller's reset line may have
 * glitched while the chip was waking, so reset it and send it back to sleep rather than
 * trusting it is still asleep (an awake controller draws milliamps).
 */
[[noreturn]] void back_to_sleep()
{
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_RST);
    i2c_bus_init();
    touch_init();   // reset, and wait for its firmware to start
    touch_sleep();
    sleep_now();
}
}  // namespace

void power_boot_check()
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
        release_holds();
        return;
    }
    // Woken by the button. Everything stays dark until it has been held long enough.
    hold_level(PIN_LCD_BL, 0);
    rtc_gpio_deinit((gpio_num_t)PIN_BUTTON);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    uint32_t up_since = 0;
    while (millis() < POWER_ON_HOLD_MS) {
        if (digitalRead(PIN_BUTTON) == HIGH) {
            if (!up_since) up_since = millis();
            else if (millis() - up_since >= RELEASE_DEBOUNCE_MS) back_to_sleep();   // let go too soon
        } else {
            up_since = 0;
        }
        delay(5);
    }
    release_holds();
}

void power_off()
{
    // Sleep only once the button is up, or its low level would wake the chip straight away.
    wait_for_release();
    touch_sleep();
    display_power_off();
    sleep_now();
}
