#pragma once
/**
 * lgfx.hpp – LovyanGFX driver for ILI9341 on ESP32-S3
 *
 * Display : 240 × 320, 16-bit RGB565
 * SPI     : SPI3_HOST, 40 MHz write, 16 MHz read
 * Pins    : SCLK=12, MOSI=11, MISO=13, DC=46, CS=10, BL=45 (PWM)
 */

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX() {
        /* ── SPI bus ─────────────────────────────────────────── */
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI3_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.pin_sclk   = 12;
            cfg.pin_mosi   = 11;
            cfg.pin_miso   = 13;
            cfg.pin_dc     = 46;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        /* ── Panel ───────────────────────────────────────────── */
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 10;
            cfg.pin_rst  = -1;
            cfg.pin_busy = -1;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable    = true;
            cfg.invert      = true;
            cfg.rgb_order   = true;
            cfg.dlen_16bit  = false;
            cfg.bus_shared  = true;
            _panel_instance.config(cfg);
        }

        /* ── Backlight (PWM) ─────────────────────────────────── */
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 45;
            cfg.invert      = true;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};
