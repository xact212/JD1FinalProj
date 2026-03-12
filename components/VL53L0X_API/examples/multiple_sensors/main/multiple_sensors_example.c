/* VL53L0X Multiple Sensors Example
 * 
 * This example demonstrates how to use multiple VL53L0X sensors on the same I2C bus
 * using the new API (vl53l0x_config_with_bus)
 * 
 * Key points:
 * - All sensors share the same I2C bus
 * - Each sensor needs a unique I2C address
 * - XSHUT pins are used to change addresses at initialization
 * 
 * Hardware connections:
 * Sensor 1:
 * - VL53L0X SCL/SDA -> Shared I2C bus (GPIO 22/21)
 * - VL53L0X XSHUT -> GPIO 23
 * 
 * Sensor 2:
 * - VL53L0X SCL/SDA -> Shared I2C bus (GPIO 22/21)
 * - VL53L0X XSHUT -> GPIO 25
 * 
 * Sensor 3:
 * - VL53L0X SCL/SDA -> Shared I2C bus (GPIO 22/21)
 * - VL53L0X XSHUT -> GPIO 26
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "vl53l0x.h"

static const char *TAG = "VL53L0X_MULTI";

// I2C Configuration
#define I2C_MASTER_NUM      0
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21

// Sensor configuration
#define NUM_SENSORS 3

// XSHUT pins for each sensor (must be different)
#define SENSOR1_XSHUT 23
#define SENSOR2_XSHUT 25
#define SENSOR3_XSHUT 26

// New I2C addresses (must be unique, default is 0x29)
#define SENSOR1_ADDR 0x30
#define SENSOR2_ADDR 0x31
#define SENSOR3_ADDR 0x32

void app_main(void)
{
    ESP_LOGI(TAG, "VL53L0X Multiple Sensors Example");
    
    // Step 1: Create a single shared I2C bus
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "I2C bus created successfully");
    
    // Step 2: Initialize sensors one at a time with unique addresses
    // We use XSHUT pins to keep sensors in reset while configuring others
    vl53l0x_t *sensors[NUM_SENSORS];
    int8_t xshut_pins[NUM_SENSORS] = {SENSOR1_XSHUT, SENSOR2_XSHUT, SENSOR3_XSHUT};
    uint8_t addresses[NUM_SENSORS] = {SENSOR1_ADDR, SENSOR2_ADDR, SENSOR3_ADDR};
    
    // First, hold all sensors in reset
    for (int i = 0; i < NUM_SENSORS; i++) {
        gpio_reset_pin(xshut_pins[i]);
        gpio_set_direction(xshut_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(xshut_pins[i], 0);  // Keep in reset
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "All sensors held in reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize each sensor one by one
    for (int i = 0; i < NUM_SENSORS; i++) {
        ESP_LOGI(TAG, "\nInitializing sensor %d...", i + 1);
        
        // Create sensor with shared bus and default address (0x29)
        sensors[i] = vl53l0x_config_with_bus(
            bus_handle,
            xshut_pins[i],
            0x29,  // All start with default address
            1      // Use 2.8V I/O mode
        );
        
        if (!sensors[i]) {
            ESP_LOGE(TAG, "Failed to configure sensor %d", i + 1);
            continue;
        }
        
        // Release this sensor from reset
        gpio_set_level(xshut_pins[i], 1);
        vTaskDelay(pdMS_TO_TICKS(10));  // Wait for sensor to boot
        
        // Initialize the sensor
        const char *err = vl53l0x_init(sensors[i]);
        if (err) {
            ESP_LOGE(TAG, "Failed to initialize sensor %d: %s", i + 1, err);
            vl53l0x_end(sensors[i]);
            sensors[i] = NULL;
            continue;
        }
        
        // Change to unique I2C address
        vl53l0x_setAddress(sensors[i], addresses[i]);
        ESP_LOGI(TAG, "Sensor %d initialized with address 0x%02X", i + 1, addresses[i]);
        
        // Optional: Configure timing budget
        vl53l0x_setMeasurementTimingBudget(sensors[i], 40000);  // 40ms
    }
    
    ESP_LOGI(TAG, "\nAll sensors initialized!\n");
    
    // Step 3: Start continuous measurements on all sensors
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (sensors[i]) {
            vl53l0x_startContinuous(sensors[i], 100);  // 100ms interval
        }
    }
    
    // Step 4: Read from all sensors in a loop
    ESP_LOGI(TAG, "Starting continuous measurements from all sensors...");
    
    for (int iteration = 0; iteration < 30; iteration++) {
        ESP_LOGI(TAG, "\n--- Measurement %d ---", iteration + 1);
        
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (!sensors[i]) {
                ESP_LOGW(TAG, "Sensor %d: Not available", i + 1);
                continue;
            }
            
            uint16_t range_mm = vl53l0x_readRangeContinuousMillimeters(sensors[i]);
            
            if (vl53l0x_timeoutOccurred(sensors[i])) {
                ESP_LOGW(TAG, "Sensor %d: Timeout", i + 1);
            } else if (vl53l0x_i2cFail(sensors[i])) {
                ESP_LOGE(TAG, "Sensor %d: I2C error", i + 1);
            } else {
                ESP_LOGI(TAG, "Sensor %d: %d mm", i + 1, range_mm);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    
    // Step 5: Stop and cleanup
    ESP_LOGI(TAG, "\nStopping measurements and cleaning up...");
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (sensors[i]) {
            vl53l0x_stopContinuous(sensors[i]);
            vl53l0x_end(sensors[i]);  // Does NOT delete the shared bus
        }
    }
    
    // Delete the shared bus only after all sensors are done
    i2c_del_master_bus(bus_handle);
    
    ESP_LOGI(TAG, "Example finished");
}
