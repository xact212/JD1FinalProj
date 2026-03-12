/* VL53L0X Single Sensor Example
 * 
 * This example demonstrates basic usage of a single VL53L0X sensor
 * using the legacy API (vl53l0x_config)
 * 
 * Hardware connections:
 * - VL53L0X VCC -> 3.3V or 5V (depending on module)
 * - VL53L0X GND -> GND
 * - VL53L0X SCL -> GPIO 22 (or your chosen SCL pin)
 * - VL53L0X SDA -> GPIO 21 (or your chosen SDA pin)
 * - VL53L0X XSHUT -> GPIO 23 (optional, for power control)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "vl53l0x.h"

static const char *TAG = "VL53L0X_SINGLE";

// I2C Configuration
#define I2C_MASTER_NUM      0
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21
#define VL53L0X_XSHUT_PIN   23  // Set to -1 if not using XSHUT
#define VL53L0X_ADDRESS     0x29  // Default I2C address

void app_main(void)
{
    ESP_LOGI(TAG, "VL53L0X Single Sensor Example");
    
    // Initialize VL53L0X sensor with legacy API
    // This creates and manages its own I2C bus
    vl53l0x_t *sensor = vl53l0x_config(
        I2C_MASTER_NUM,
        I2C_MASTER_SCL_IO,
        I2C_MASTER_SDA_IO,
        VL53L0X_XSHUT_PIN,
        VL53L0X_ADDRESS,
        1  // Use 2.8V I/O mode (set to 0 for 1.8V)
    );
    
    if (!sensor) {
        ESP_LOGE(TAG, "Failed to configure VL53L0X sensor");
        return;
    }
    
    ESP_LOGI(TAG, "VL53L0X configured successfully");
    
    // Initialize the sensor
    const char *err = vl53l0x_init(sensor);
    if (err) {
        ESP_LOGE(TAG, "VL53L0X initialization failed: %s", err);
        vl53l0x_end(sensor);
        return;
    }
    
    ESP_LOGI(TAG, "VL53L0X initialized successfully");
    
    // Optional: Set timing budget (default is ~33ms)
    // Higher budget = more accurate but slower
    err = vl53l0x_setMeasurementTimingBudget(sensor, 50000);  // 50ms
    if (err) {
        ESP_LOGW(TAG, "Failed to set timing budget: %s", err);
    }
    
    // Example 1: Single shot measurements
    ESP_LOGI(TAG, "\n=== Single Shot Mode ===");
    for (int i = 0; i < 5; i++) {
        uint16_t range_mm = vl53l0x_readRangeSingleMillimeters(sensor);
        
        if (vl53l0x_timeoutOccurred(sensor)) {
            ESP_LOGW(TAG, "Measurement timeout!");
        } else {
            ESP_LOGI(TAG, "Range: %d mm", range_mm);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Example 2: Continuous measurements
    ESP_LOGI(TAG, "\n=== Continuous Mode ===");
    ESP_LOGI(TAG, "Starting continuous ranging (200ms interval)...");
    
    vl53l0x_startContinuous(sensor, 200);  // 200ms between measurements
    
    for (int i = 0; i < 20; i++) {
        uint16_t range_mm = vl53l0x_readRangeContinuousMillimeters(sensor);
        
        if (vl53l0x_timeoutOccurred(sensor)) {
            ESP_LOGW(TAG, "Measurement timeout!");
        } else {
            ESP_LOGI(TAG, "Range: %d mm", range_mm);
        }
        
        // Check for I2C errors
        if (vl53l0x_i2cFail(sensor)) {
            ESP_LOGE(TAG, "I2C communication error!");
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(250));  // Slightly longer than measurement interval
    }
    
    vl53l0x_stopContinuous(sensor);
    ESP_LOGI(TAG, "Continuous ranging stopped");
    
    // Example 3: High-speed continuous mode (back-to-back)
    ESP_LOGI(TAG, "\n=== High-Speed Mode ===");
    ESP_LOGI(TAG, "Starting back-to-back continuous ranging...");
    
    vl53l0x_startContinuous(sensor, 0);  // 0 = back-to-back mode (fastest)
    
    for (int i = 0; i < 10; i++) {
        uint16_t range_mm = vl53l0x_readRangeContinuousMillimeters(sensor);
        ESP_LOGI(TAG, "Range: %d mm", range_mm);
        vTaskDelay(pdMS_TO_TICKS(50));  // Small delay
    }
    
    vl53l0x_stopContinuous(sensor);
    
    // Cleanup
    vl53l0x_end(sensor);
    ESP_LOGI(TAG, "Example finished");
}
