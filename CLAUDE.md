# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-C6 oven temperature controller with touchscreen UI. Built with ESP-IDF v5.5.2 and LVGL v8.4.0.

## Build Commands

```bash
idf.py build                          # Build
idf.py flash -p /dev/ttyACM0          # Flash
idf.py monitor -p /dev/ttyACM0        # Serial monitor
idf.py -p /dev/ttyACM0 flash monitor  # Build, flash, monitor
idf.py fullclean                      # Clean build
```

## Architecture

**Entry point**: `main/ovenctrl.c` - initializes hardware, starts FreeRTOS tasks

**Components** (`components/`):
- `display/` - SPI display driver (ILI9341), LVGL flush callback
- `touch/` - I2C capacitive touch, LVGL input handler
- `max31855/` - Dual thermocouple reader (software SPI)
- `io_expander/` - PCF8574 I2C GPIO expander for heater/fan outputs
- `pid_controller/` - PID temperature control with auto-calibration, NVS persistence
- `wifi_manager/` - WiFi scan/connect, credential storage in NVS
- `ui/` - LVGL screens (main, settings, wifi, calibration, keyboard)

**FreeRTOS tasks**: `lv_tick` and `lvgl` (LVGL timing/events), `thermocouple` (temperature reading)

**Hardware pins**: Defined in `pinconfig` file. Touch and IO expander share I2C bus at 400kHz.

## Key Patterns

- Set `DISABLE_TEMPERATURE_READING` in `main/ovenctrl.c` to test without hardware
- Use ESP-IDF logging: `ESP_LOGI(TAG, "message")`, `ESP_LOGE(TAG, "error")`
- NVS namespaces: `wifi_creds` for WiFi, `pid_params` for calibration
- UI screens created on demand via `ui_manager.c`; main screen at startup
