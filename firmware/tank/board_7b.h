// Hardware driver for the Waveshare ESP32-S3-Touch-LCD-7B / ESP32-S3-LCD-7B (1024x600).
// You shouldn't need to edit this file.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "settings.h"

#ifndef BACKLIGHT_INVERTED
#define BACKLIGHT_INVERTED 1
#endif
#ifndef SCREEN_BUFFERS
#define SCREEN_BUFFERS 3
#endif
#ifndef SWAP_RED_BLUE
#define SWAP_RED_BLUE 0
#endif

namespace board {

// ---- I/O expander (Waveshare CH32V003 firmware at I2C address 0x24)
static const uint8_t EXIO_ADDR = 0x24;
static const uint8_t REG_MODE = 0x02, REG_OUT = 0x03, REG_PWM = 0x05;
static const uint8_t EXIO_TP_RST = 1, EXIO_BL_EN = 2, EXIO_LCD_RST = 3, EXIO_SD_CS = 4, EXIO_USB_SEL = 5, EXIO_LCD_VDD = 6;
static uint8_t exioOut = 0;

static bool exioWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(EXIO_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static void exioPin(uint8_t pin, bool high) {
  if (high) exioOut |= (1 << pin); else exioOut &= ~(1 << pin);
  exioWrite(REG_OUT, exioOut);
}

// brightness 0..100 percent
static void setBacklight(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  int reg = BACKLIGHT_INVERTED ? (int)lroundf((1.0f - percent / 100.0f) * 246) + 1
                               : (int)lroundf(percent / 100.0f * 246) + 1;
  exioWrite(REG_PWM, (uint8_t)reg);
  exioPin(EXIO_BL_EN, percent > 0);
}

// ---- RGB panel
static const int NUM_FB = SCREEN_BUFFERS < 2 ? 2 : SCREEN_BUFFERS > 3 ? 3 : SCREEN_BUFFERS;
static esp_lcd_panel_handle_t panel = nullptr;
static SemaphoreHandle_t vsyncSem = nullptr;
static uint16_t* fb[3] = { nullptr, nullptr, nullptr };
static volatile uint32_t vsyncCount = 0;
static uint32_t presentedAt[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };  // vsyncCount when each was shown

static bool IRAM_ATTR onVsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void*) {
  vsyncCount = vsyncCount + 1;  // only this interrupt writes it
  BaseType_t woke = pdFALSE;
  xSemaphoreGiveFromISR(vsyncSem, &woke);
  return woke == pdTRUE;
}

// Panel timings (pixels / lines), from Waveshare's documentation.
static const int H_PULSE = 162, H_BACK = 152, H_FRONT = 48, V_PULSE = 45, V_BACK = 13, V_FRONT = 3;

// How many times a second the panel redraws itself.
static float refreshHz() {
  return LCD_PCLK_HZ / float((1024 + H_PULSE + H_BACK + H_FRONT) * (600 + V_PULSE + V_BACK + V_FRONT));
}

static bool begin() {
  Wire.begin(8, 9, 400000);
  if (!exioWrite(REG_MODE, 0xFF)) Serial.println("[board] I/O expander not answering at 0x24");
  // power the panel, hold it in reset, keep USB (not CAN) selected
  exioOut = (1 << EXIO_TP_RST) | (1 << EXIO_SD_CS) | (1 << EXIO_LCD_VDD);
  exioWrite(REG_OUT, exioOut);
  delay(20);
  exioPin(EXIO_LCD_RST, true);
  delay(60);

  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = LCD_PCLK_HZ;
  cfg.timings.h_res = 1024;
  cfg.timings.v_res = 600;
  cfg.timings.hsync_pulse_width = H_PULSE;
  cfg.timings.hsync_back_porch = H_BACK;
  cfg.timings.hsync_front_porch = H_FRONT;
  cfg.timings.vsync_pulse_width = V_PULSE;
  cfg.timings.vsync_back_porch = V_BACK;
  cfg.timings.vsync_front_porch = V_FRONT;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = NUM_FB;
  cfg.bounce_buffer_size_px = 1024 * 10;
  cfg.hsync_gpio_num = 46;
  cfg.vsync_gpio_num = 3;
  cfg.de_gpio_num = 5;
  cfg.pclk_gpio_num = 7;
  cfg.disp_gpio_num = -1;
  const int blue[5] = { 14, 38, 18, 17, 10 };          // B3..B7
  const int green[6] = { 39, 0, 45, 48, 47, 21 };      // G2..G7
  const int red[5] = { 1, 2, 42, 41, 40 };             // R3..R7
  // data lines 0-4 carry blue, 5-10 green, 11-15 red (SWAP_RED_BLUE swaps red and blue)
  for (int i = 0; i < 5; i++) {
    cfg.data_gpio_nums[i] = SWAP_RED_BLUE ? red[i] : blue[i];
    cfg.data_gpio_nums[11 + i] = SWAP_RED_BLUE ? blue[i] : red[i];
  }
  for (int i = 0; i < 6; i++) cfg.data_gpio_nums[5 + i] = green[i];
  cfg.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&cfg, &panel) != ESP_OK) { Serial.println("[board] RGB panel create failed (is PSRAM enabled?)"); return false; }
  vsyncSem = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr);
  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  void *a = nullptr, *b = nullptr, *c = nullptr;
  if (NUM_FB == 3) esp_lcd_rgb_panel_get_frame_buffer(panel, 3, &a, &b, &c);
  else esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &a, &b);
  fb[0] = (uint16_t*)a;
  fb[1] = (uint16_t*)b;
  fb[2] = (uint16_t*)c;
  Serial.printf("[board] %d screen buffers, refresh %.1f Hz\n", NUM_FB, refreshHz());
  return fb[0] && fb[1] && (NUM_FB == 2 || fb[2]);
}

// Hand a finished buffer to the screen. It goes on screen at the next refresh.
static void present(int k) {
  presentedAt[k] = vsyncCount;
  esp_lcd_panel_draw_bitmap(panel, 0, 0, 1024, 600, fb[k]);
}

// Wait until buffer k may be drawn into: it must be off screen, which happens once the
// buffer shown after it has actually gone up. With 2 buffers that's always a wait for the
// next refresh; with 3 we usually don't wait at all, so a frame that takes a bit longer
// than one refresh costs a little speed instead of halving the frame rate.
static void waitUntilFree(int k) {
  int after = (k + 1) % NUM_FB;
  if (presentedAt[after] == 0xFFFFFFFF) return;  // never shown yet
  uint32_t t0 = millis();
  while ((int32_t)(vsyncCount - presentedAt[after]) <= 0 && millis() - t0 < 100)
    xSemaphoreTake(vsyncSem, pdMS_TO_TICKS(20));
}

// ---- touchscreen (GT911 chip, only on the touch version of the board)
// Shares the I2C wires with the I/O expander. Its reset line is EXIO1, which begin() already
// releases. Depending on how it starts it answers at 0x5D or 0x14, so both are tried.
static uint8_t touchAddr = 0;
static int touchMaxX = 1024, touchMaxY = 600;
static bool touchDown = false;
static int touchX = 0, touchY = 0;

static bool gtRead(uint16_t reg, uint8_t* buf, int n) {
  Wire.beginTransmission(touchAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)touchAddr, n) != n) return false;
  for (int i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}
static void gtWrite(uint16_t reg, uint8_t v) {
  Wire.beginTransmission(touchAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(v);
  Wire.endTransmission();
}

static bool touchBegin() {
  const uint8_t addrs[2] = { 0x5D, 0x14 };
  for (uint8_t a : addrs) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { touchAddr = a; break; }
  }
  if (!touchAddr) { Serial.println("[touch] no touchscreen found (normal for the board without touch)"); return false; }
  uint8_t c[4];
  if (gtRead(0x8048, c, 4)) {  // the touch area size it's set up for
    int mx = c[0] | (c[1] << 8), my = c[2] | (c[3] << 8);
    if (mx > 0 && my > 0) { touchMaxX = mx; touchMaxY = my; }
  }
  Serial.printf("[touch] touchscreen found (GT911 at 0x%02X, %d x %d)\n", touchAddr, touchMaxX, touchMaxY);
  return true;
}

// Is a finger on the screen? If so, x and y are where (in screen pixels).
static bool touchRead(int& x, int& y) {
  if (!touchAddr) return false;
  uint8_t st;
  if (gtRead(0x814E, &st, 1) && (st & 0x80)) {  // new data from the chip
    touchDown = false;
    if ((st & 0x0F) > 0) {
      uint8_t p[4];
      if (gtRead(0x8150, p, 4)) {
        int tx = p[0] | (p[1] << 8), ty = p[2] | (p[3] << 8);
        if (TOUCH_SWAP_XY) { int s = tx; tx = ty; ty = s; }
        int mx = TOUCH_SWAP_XY ? touchMaxY : touchMaxX, my = TOUCH_SWAP_XY ? touchMaxX : touchMaxY;
        tx = tx * 1024 / (mx > 0 ? mx : 1024);
        ty = ty * 600 / (my > 0 ? my : 600);
        if (TOUCH_FLIP_X) tx = 1023 - tx;
        if (TOUCH_FLIP_Y) ty = 599 - ty;
        touchX = tx < 0 ? 0 : tx > 1023 ? 1023 : tx;
        touchY = ty < 0 ? 0 : ty > 599 ? 599 : ty;
        touchDown = true;
      }
    }
    gtWrite(0x814E, 0);  // tell the chip we've read it
  }
  x = touchX; y = touchY;
  return touchDown;
}

// Room for some pixels in the big (PSRAM) memory.
static uint16_t* allocPixels(size_t count) {
  return (uint16_t*)heap_caps_malloc(count * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static uint16_t* allocScreen() {
  return (uint16_t*)heap_caps_malloc(1024 * 600 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

}  // namespace board
