# ESP32-VL53L0X

VL53L0X laser ranging sensor library for ESP-IDF (ESP32, ESP32-S2, ESP32-C3, etc.)

## Overview

This library provides a simple interface to the ST VL53L0X Time-of-Flight (ToF) ranging sensor using ESP-IDF v5.2+ with the new I2C master driver API.
This is a fork from revk's library for the same, but tweaked for the new I2C Master driver API, and tweaked to handle multiple devices, the original
code wouldn't actually do that. 

## Features

- Support for ESP-IDF v5.2 and newer
- Uses the new I2C master bus/device driver API
- Single and continuous ranging modes
- Multiple sensor support on same I2C bus
- Configurable timing budgets and VCSEL periods
- XSHUT pin control for sensor management

## Requirements

- **ESP-IDF v5.2 or newer** - This library uses the new I2C master driver API introduced in ESP-IDF v5.2
- VL53L0X sensor module

## Installation

Add this component to your ESP-IDF project:

```bash
cd components
git clone https://github.com/bigredelf/ESP32-VL53L0X.git
```

Or add as a git submodule:

```bash
git submodule add https://github.com/bigredelf/ESP32-VL53L0X.git components/ESP32-VL53L0X
```

## Quick Start

### Single Sensor Example

```c
#include <stdio.h>
#include "vl53l0x.h"
#include "esp_log.h"

#define I2C_PORT    0
#define I2C_SDA     21
#define I2C_SCL     22
#define XSHUT_PIN   -1  // Not used

void app_main(void)
{
    // Initialize the sensor
    vl53l0x_t *sensor = vl53l0x_config(I2C_PORT, I2C_SCL, I2C_SDA, XSHUT_PIN, 0x29, 1);
    if (!sensor) {
        ESP_LOGE("VL53L0X", "Failed to configure sensor");
        return;
    }

    const char *err = vl53l0x_init(sensor);
    if (err) {
        ESP_LOGE("VL53L0X", "Init failed: %s", err);
        vl53l0x_end(sensor);
        return;
    }

    // Start continuous ranging
    vl53l0x_startContinuous(sensor, 0);

    while (1) {
        uint16_t range = vl53l0x_readRangeContinuousMillimeters(sensor);
        if (range == 65535) {
            ESP_LOGW("VL53L0X", "Out of range or timeout");
        } else {
            ESP_LOGI("VL53L0X", "Range: %d mm", range);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Multiple Sensors Example

For multiple sensors on the same I2C bus, use XSHUT pins to enable them one at a time and assign unique addresses:

```c
#include "vl53l0x.h"
#include <driver/i2c_master.h>

#define I2C_PORT    0
#define I2C_SDA     21
#define I2C_SCL     22
#define XSHUT1      4
#define XSHUT2      5

void app_main(void)
{
    // Create shared I2C bus
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = I2C_SCL,
        .sda_io_num = I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&bus_config, &bus_handle);

    // Initialize first sensor with default address
    vl53l0x_t *sensor1 = vl53l0x_config_with_bus(bus_handle, XSHUT1, 0x29, 1);
    vl53l0x_init(sensor1);
    vl53l0x_setAddress(sensor1, 0x30);  // Change address

    // Initialize second sensor
    vl53l0x_t *sensor2 = vl53l0x_config_with_bus(bus_handle, XSHUT2, 0x29, 1);
    vl53l0x_init(sensor2);
    vl53l0x_setAddress(sensor2, 0x31);  // Different address

    // Start continuous ranging on both
    vl53l0x_startContinuous(sensor1, 0);
    vl53l0x_startContinuous(sensor2, 0);

    while (1) {
        uint16_t range1 = vl53l0x_readRangeContinuousMillimeters(sensor1);
        uint16_t range2 = vl53l0x_readRangeContinuousMillimeters(sensor2);
        ESP_LOGI("VL53L0X", "Sensor1: %d mm, Sensor2: %d mm", range1, range2);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## API Reference

### Configuration Functions

#### `vl53l0x_config()`
Create and configure a VL53L0X sensor with its own I2C bus.

```c
vl53l0x_t *vl53l0x_config(int8_t port, int8_t scl, int8_t sda, 
                          int8_t xshut, uint8_t address, uint8_t io_2v8);
```

**Parameters:**
- `port` - I2C port number (0 or 1)
- `scl` - SCL GPIO pin number
- `sda` - SDA GPIO pin number
- `xshut` - XSHUT GPIO pin number (-1 if not used)
- `address` - I2C address (default: 0x29)
- `io_2v8` - Set to 1 for 2.8V I/O, 0 for 1.8V

**Returns:** Pointer to vl53l0x_t structure, or NULL on failure

#### `vl53l0x_config_with_bus()`
Create a VL53L0X sensor using an existing I2C bus handle (for multiple sensors).

```c
vl53l0x_t *vl53l0x_config_with_bus(i2c_master_bus_handle_t bus_handle, 
                                   int8_t xshut, uint8_t address, uint8_t io_2v8);
```

**Parameters:**
- `bus_handle` - Existing I2C master bus handle
- `xshut` - XSHUT GPIO pin number (-1 if not used)
- `address` - I2C address (default: 0x29)
- `io_2v8` - Set to 1 for 2.8V I/O, 0 for 1.8V

**Returns:** Pointer to vl53l0x_t structure, or NULL on failure

#### `vl53l0x_init()`
Initialize the sensor with default settings.

```c
const char *vl53l0x_init(vl53l0x_t *v);
```

**Returns:** NULL on success, error string on failure

#### `vl53l0x_end()`
Clean up and free resources.

```c
void vl53l0x_end(vl53l0x_t *v);
```

### Ranging Functions

#### `vl53l0x_readRangeSingleMillimeters()`
Perform a single ranging measurement.

```c
uint16_t vl53l0x_readRangeSingleMillimeters(vl53l0x_t *v);
```

**Returns:** Range in millimeters, or 65535 on error/timeout

#### `vl53l0x_startContinuous()`
Start continuous ranging mode.

```c
void vl53l0x_startContinuous(vl53l0x_t *v, uint32_t period_ms);
```

**Parameters:**
- `period_ms` - Inter-measurement period in milliseconds (0 for back-to-back mode)

#### `vl53l0x_readRangeContinuousMillimeters()`
Read range in continuous mode.

```c
uint16_t vl53l0x_readRangeContinuousMillimeters(vl53l0x_t *v);
```

**Returns:** Range in millimeters, or 65535 on error/timeout

#### `vl53l0x_stopContinuous()`
Stop continuous ranging mode.

```c
void vl53l0x_stopContinuous(vl53l0x_t *v);
```

### Configuration Functions

#### `vl53l0x_setAddress()`
Change the I2C address of the sensor.

```c
void vl53l0x_setAddress(vl53l0x_t *v, uint8_t new_addr);
```

#### `vl53l0x_setTimeout()`
Set I/O timeout in milliseconds.

```c
void vl53l0x_setTimeout(vl53l0x_t *v, uint16_t timeout_ms);
```

#### `vl53l0x_setMeasurementTimingBudget()`
Set measurement timing budget in microseconds (20000 to 1000000).

```c
const char *vl53l0x_setMeasurementTimingBudget(vl53l0x_t *v, uint32_t budget_us);
```

#### `vl53l0x_setSignalRateLimit()`
Set signal rate limit in MCPS (0.0 to 511.99).

```c
const char *vl53l0x_setSignalRateLimit(vl53l0x_t *v, float limit_Mcps);
```

#### `vl53l0x_setVcselPulsePeriod()`
Set VCSEL pulse period.

```c
const char *vl53l0x_setVcselPulsePeriod(vl53l0x_t *v, 
                                        vl53l0x_vcselPeriodType type, 
                                        uint8_t period_pclks);
```

**Parameters:**
- `type` - `VcselPeriodPreRange` or `VcselPeriodFinalRange`
- `period_pclks` - Period in PCLKs (pre-range: 12-18 even, final-range: 8-14 even)

### Status Functions

#### `vl53l0x_timeoutOccurred()`
Check if a timeout occurred during the last operation.

```c
int vl53l0x_timeoutOccurred(vl53l0x_t *v);
```

**Returns:** 1 if timeout occurred, 0 otherwise

#### `vl53l0x_i2cFail()`
Check if an I2C communication failure occurred.

```c
int vl53l0x_i2cFail(vl53l0x_t *v);
```

**Returns:** 1 if I2C failure occurred, 0 otherwise

## Hardware Connections

| VL53L0X Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| VDD | 3.3V | Power supply (2.6V - 3.5V) |
| GND | GND | Ground |
| SCL | GPIO 22 | I2C clock (configurable) |
| SDA | GPIO 21 | I2C data (configurable) |
| XSHUT | GPIO (optional) | Shutdown control (active low) |
| GPIO1 | Not connected | Interrupt output (optional) |

**Note:** The VL53L0X operates at 2.8V I/O by default in this library. If your module has level shifters, set `io_2v8 = 1`.

## Migration from Legacy I2C Driver

If you're migrating from the old ESP-IDF I2C driver (pre-v5.2), note these key changes:

### Old API (ESP-IDF < v5.2)
```c
vl53l0x_t *sensor = vl53l0x_config(0, 22, 21, -1, 0x29, 1);
```

### New API (ESP-IDF >= v5.2)
```c
// Same function call - internal implementation updated
vl53l0x_t *sensor = vl53l0x_config(0, 22, 21, -1, 0x29, 1);

// Or use the new bus-based API for multiple sensors
i2c_master_bus_handle_t bus_handle;
// ... create bus ...
vl53l0x_t *sensor = vl53l0x_config_with_bus(bus_handle, -1, 0x29, 1);
```

The high-level API remains the same, but internally uses the new I2C master driver.

## Examples

See the `examples/` directory for complete working examples:

- **single_sensor** - Basic single sensor ranging example
- **multiple_sensors** - Multiple sensors on same I2C bus with address management

## Troubleshooting

### Sensor not responding
- Check power supply (2.6V - 3.5V)
- Verify I2C connections (SDA, SCL)
- Ensure pull-up resistors are present (usually on module)
- Check I2C address (default 0x29)

### Timeout errors
- Increase timeout: `vl53l0x_setTimeout(sensor, 500);`
- Check for I2C bus conflicts
- Verify sensor is powered and initialized

### Range readings are unstable
- Adjust measurement timing budget
- Change VCSEL pulse period
- Ensure target surface is suitable (non-reflective works best)

## Credits

Based on the original work by:
- Copyright © 2019 Adrian Kennard, Andrews & Arnold Ltd
- Based on [Pololu VL53L0X Arduino library](https://github.com/pololu/vl53l0x-arduino)
- Updated for ESP-IDF v5.2+ I2C master driver API

## License

GPL 3.0 - See LICENSE file for details
