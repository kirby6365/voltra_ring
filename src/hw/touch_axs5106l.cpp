#include "touch_axs5106l.h"

#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"
#include "i2c_bus.h"

namespace {
constexpr uint8_t REG_TOUCH_DATA = 0x01;   // [0] gesture, [1] point count, then 6 bytes per point
constexpr uint8_t REG_CHIP_ID    = 0x08;
// Not in the datasheets we have; from the mydazy/esp_lcd_touch_axs5106l driver.
constexpr uint8_t REG_SLEEP      = 0x19;
constexpr uint8_t SLEEP_ENTER    = 0x03;
constexpr uint8_t REG_SOFT_RESET = 0xF0;
constexpr uint8_t SOFT_RESET_KEY[] = {0xB3, 0x55, 0xAA, 0x34, 0x01};
constexpr int INIT_ATTEMPTS = 3;

bool read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    Wire.beginTransmission(I2C_ADDR_AXS5106L);
    Wire.write(reg);
    // Stop after the register write, then read: the same transaction shape as the
    // Waveshare driver.
    if (Wire.endTransmission(true) != 0) {
        return false;
    }
    size_t got = Wire.requestFrom((uint8_t)I2C_ADDR_AXS5106L, (uint8_t)len);
    if (got != len) {
        while (Wire.available()) Wire.read();
        return false;
    }
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

/**
 * Soft reset over I2C, then a reset pulse. The reset pin alone does not bring the
 * controller out of the sleep mode touch_sleep() leaves it in; the I2C write wakes it.
 * Sent blind: a controller that is already running may not ack it.
 */
void reset_controller()
{
    {
        I2cLock lock;
        Wire.beginTransmission(I2C_ADDR_AXS5106L);
        Wire.write(REG_SOFT_RESET);
        Wire.write(SOFT_RESET_KEY, sizeof(SOFT_RESET_KEY));
        Wire.endTransmission(true);
    }
    digitalWrite(PIN_TOUCH_RST, LOW);
    delay(20);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(200);   // the controller boots its firmware after reset
}
}  // namespace

bool touch_init()
{
    pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
    pinMode(PIN_TOUCH_RST, OUTPUT);
    digitalWrite(PIN_TOUCH_RST, HIGH);

    uint8_t id[3] = {0};
    bool ok = false;
    for (int i = 0; i < INIT_ATTEMPTS && !ok; i++) {
        reset_controller();
        I2cLock lock;
        ok = read_regs(REG_CHIP_ID, id, sizeof(id));
    }
    log_i("AXS5106L id %02x %02x %02x (%s)", id[0], id[1], id[2], ok ? "ok" : "no ack");
    return ok;
}

void touch_sleep()
{
    I2cLock lock;
    Wire.beginTransmission(I2C_ADDR_AXS5106L);
    Wire.write(REG_SLEEP);
    Wire.write(SLEEP_ENTER);
    if (Wire.endTransmission(true) != 0) log_w("AXS5106L sleep: no ack");
}

bool touch_read(uint16_t &x, uint16_t &y)
{
    uint8_t d[14] = {0};
    {
        I2cLock lock;
        if (!read_regs(REG_TOUCH_DATA, d, sizeof(d))) {
            return false;
        }
    }
    if ((d[1] & 0x0F) == 0) {
        return false;
    }
    const uint16_t rx = ((uint16_t)(d[2] & 0x0F) << 8) | d[3];
    const uint16_t ry = ((uint16_t)(d[4] & 0x0F) << 8) | d[5];
    if (rx >= LCD_H_RES || ry >= LCD_V_RES) {
        return false;
    }
    // The controller's X runs the opposite way to the panel's columns.
#if LCD_ROTATE_180
    x = rx;
    y = LCD_V_RES - 1 - ry;
#else
    x = LCD_H_RES - 1 - rx;
    y = ry;
#endif
    return true;
}
