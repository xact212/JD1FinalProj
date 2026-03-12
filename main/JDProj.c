#include <stdio.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "rom/gpio.h"

#include "vl53l0x.h"
#include "i2c-lcd1602.h"
#include "smbus.h"

static const char* TAG = "JDProj";

#define I2C_MASTER_SCL 22 
#define I2C_MASTER_SDA 21
#define I2C_MASTER_PORT I2C_NUM_0 
#define I2C_MASTER_FREQ 50000  // Keep at 50kHz for reliability
#define LCD_ADDR 0x27  // Your LCD address
#define VL53L0X_ADDR 0x29

// Diagnostic LED pins (use built-in LED if available, or add your own)
#define LED_PIN 2  // GPIO2 usually has built-in LED on ESP32 dev boards

// Simple delay function for microsecond delays
static void delay_us(uint32_t us) {
    uint32_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < us) {
        asm volatile("nop");
    }
}

// Initialize I2C with proper error checking
esp_err_t i2c_legacy_init() {
    ESP_LOGI(TAG, "Initializing legacy I2C driver...");
    
    // Uninstall any existing driver
    i2c_driver_delete(I2C_MASTER_PORT);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "I2C driver initialized");
    return ESP_OK;
}

// Simple I2C write to test a device
bool i2c_test_device(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK);
}

// Scan I2C bus and print results
void i2c_scan_bus() {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int devices_found = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_test_device(addr)) {
            ESP_LOGI(TAG, "Device found at 0x%02X", addr);
            devices_found++;
        }
        delay_us(1000);  // Small delay between scans
    }
    
    ESP_LOGI(TAG, "Scan complete. Found %d device(s)", devices_found);
}

// Ultra-simple LCD initialization - just the basics
bool init_lcd_simple() {
    ESP_LOGI(TAG, "Attempting simple LCD init at 0x%02X", LCD_ADDR);
    
    // Try to write to LCD multiple times
    for (int attempt = 0; attempt < 10; attempt++) {
        // Simple I2C write to LCD (just to see if it responds)
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0x00, true);  // Control byte
        i2c_master_write_byte(cmd, 0x30, true);  // Initialization command
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "LCD responded on attempt %d", attempt + 1);
            return true;
        } else {
            ESP_LOGW(TAG, "LCD no response on attempt %d: %s", attempt + 1, esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    return false;
}

// Proper LCD initialization using the library
bool init_lcd_proper(i2c_lcd1602_info_t** lcdInfo, smbus_info_t** smbusInfo) {
    *lcdInfo = i2c_lcd1602_malloc();
    *smbusInfo = smbus_malloc();
    
    if (!*lcdInfo || !*smbusInfo) {
        ESP_LOGE(TAG, "LCD allocation failed");
        return false;
    }
    
    // Try multiple times with different approaches
    for (int retry = 0; retry < 5; retry++) {
        ESP_LOGI(TAG, "LCD init attempt %d", retry + 1);
        
        // Reinitialize smbus
        esp_err_t espErr = smbus_init(*smbusInfo, I2C_MASTER_PORT, LCD_ADDR);
        if (espErr != ESP_OK) {
            ESP_LOGW(TAG, "smbus_init failed: %s", esp_err_to_name(espErr));
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        
        // Critical: Wait for LCD to power up
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Try library init
        espErr = i2c_lcd1602_init(*lcdInfo, *smbusInfo, 16, 2);
        if (espErr == ESP_OK) {
            ESP_LOGI(TAG, "LCD initialized successfully");
            
            // Test the LCD with simple commands
            vTaskDelay(pdMS_TO_TICKS(50));
            i2c_lcd1602_clear(*lcdInfo);
            vTaskDelay(pdMS_TO_TICKS(10));
            i2c_lcd1602_move_cursor(*lcdInfo, 0, 0);
            i2c_lcd1602_write_string(*lcdInfo, "OK");
            vTaskDelay(pdMS_TO_TICKS(10));
            
            return true;
        } else {
            ESP_LOGW(TAG, "i2c_lcd1602_init failed: %s", esp_err_to_name(espErr));
        }
        
        // Longer delay between retries
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    free(*lcdInfo);
    free(*smbusInfo);
    *lcdInfo = NULL;
    *smbusInfo = NULL;
    return false;
}

void app_main(void)
{
    // Configure LED pin for diagnostics
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
    
    ESP_LOGI(TAG, "=== RANGE DETECTOR ===");
    ESP_LOGI(TAG, "Power up sequence starting...");
    
    // Give power time to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Blink LED to indicate power up
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(LED_PIN, 0);
    
    // Initialize I2C
    if (i2c_legacy_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C");
        return;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Scan I2C bus to see what's connected
    ESP_LOGI(TAG, "=== SCANNING I2C BUS ===");
    i2c_scan_bus();
    
    // First, try simple LCD communication
    ESP_LOGI(TAG, "=== TESTING LCD COMMUNICATION ===");
    if (!i2c_test_device(LCD_ADDR)) {
        ESP_LOGE(TAG, "LCD not responding at address 0x%02X!", LCD_ADDR);
        ESP_LOGI(TAG, "Check: 1. Power (LCD needs 5V)");
        ESP_LOGI(TAG, "       2. Wiring (SDA/SCL)");
        ESP_LOGI(TAG, "       3. Address (try 0x3F)");
        
        // Fast blink LED to indicate error
        for (int i = 0; i < 10; i++) {
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        return;
    }
    
    ESP_LOGI(TAG, "LCD is responding at 0x%02X!", LCD_ADDR);
    gpio_set_level(LED_PIN, 1);  // LED on indicates LCD detected
    
    // Now try simple initialization
    ESP_LOGI(TAG, "=== ATTEMPTING SIMPLE LCD INIT ===");
    if (!init_lcd_simple()) {
        ESP_LOGW(TAG, "Simple LCD init failed, trying proper init...");
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Proper LCD initialization
    ESP_LOGI(TAG, "=== ATTEMPTING PROPER LCD INIT ===");
    i2c_lcd1602_info_t* lcdInfo = NULL;
    smbus_info_t* smbusInfo = NULL;
    
    if (!init_lcd_proper(&lcdInfo, &smbusInfo)) {
        ESP_LOGE(TAG, "Failed to initialize LCD");
        return;
    }
    
    ESP_LOGI(TAG, "LCD initialized successfully!");
    gpio_set_level(LED_PIN, 0);  // LED off during sensor init
    
    // Display message
    i2c_lcd1602_clear(lcdInfo);
    vTaskDelay(pdMS_TO_TICKS(10));
    i2c_lcd1602_move_cursor(lcdInfo, 0, 0);
    i2c_lcd1602_write_string(lcdInfo, "Range Sensor");
    i2c_lcd1602_move_cursor(lcdInfo, 0, 1);
    i2c_lcd1602_write_string(lcdInfo, "Init Sensor...");
    
    // Initialize sensor
    ESP_LOGI(TAG, "=== INITIALIZING SENSOR ===");
    vl53l0x_t* sensorHand = vl53l0x_config(
        I2C_MASTER_PORT,
        I2C_MASTER_SCL,
        I2C_MASTER_SDA,
        -1,
        VL53L0X_ADDR,
        1
    );
    
    if (!sensorHand) {
        ESP_LOGE(TAG, "Failed to configure sensor");
        i2c_lcd1602_clear(lcdInfo);
        i2c_lcd1602_move_cursor(lcdInfo, 0, 0);
        i2c_lcd1602_write_string(lcdInfo, "Sensor Error!");
        return;
    }
    
    const char* errStr = vl53l0x_init(sensorHand);
    if (errStr != NULL) {
        ESP_LOGE(TAG, "Sensor init failed: %s", errStr);
        i2c_lcd1602_clear(lcdInfo);
        i2c_lcd1602_move_cursor(lcdInfo, 0, 0);
        i2c_lcd1602_write_string(lcdInfo, "Sensor Init");
        i2c_lcd1602_move_cursor(lcdInfo, 0, 1);
        i2c_lcd1602_write_string(lcdInfo, "Failed!");
        vl53l0x_end(sensorHand);
        return;
    }
    
    ESP_LOGI(TAG, "Sensor initialized!");
    
    // Configure sensor
    vl53l0x_setMeasurementTimingBudget(sensorHand, 50000);
    vl53l0x_startContinuous(sensorHand, 200);
    
    // Update LCD
    i2c_lcd1602_clear(lcdInfo);
    vTaskDelay(pdMS_TO_TICKS(10));
    i2c_lcd1602_move_cursor(lcdInfo, 0, 0);
    i2c_lcd1602_write_string(lcdInfo, "Range:");
    
    char display_buffer[17];
    TickType_t last_lcd_update = 0;
    int error_count = 0;
    
    ESP_LOGI(TAG, "=== MAIN LOOP STARTED ===");
    gpio_set_level(LED_PIN, 1);  // LED on during operation
    
    while (true) {
        // Read sensor
        uint16_t range_mm = vl53l0x_readRangeContinuousMillimeters(sensorHand);
        
        // Check for I2C errors
        if (vl53l0x_i2cFail(sensorHand)) {
            error_count++;
            ESP_LOGE(TAG, "I2C error #%d", error_count);
            
            if (error_count > 5) {
                i2c_lcd1602_clear(lcdInfo);
                i2c_lcd1602_move_cursor(lcdInfo, 0, 0);
                i2c_lcd1602_write_string(lcdInfo, "I2C Error!");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        error_count = 0;
        
        // Update LCD every 200ms
        if (xTaskGetTickCount() - last_lcd_update >= pdMS_TO_TICKS(200)) {
            if (vl53l0x_timeoutOccurred(sensorHand)) {
                i2c_lcd1602_move_cursor(lcdInfo, 0, 1);
                i2c_lcd1602_write_string(lcdInfo, "Timeout     ");
                ESP_LOGW(TAG, "Timeout");
            } else {
                snprintf(display_buffer, sizeof(display_buffer), "%4d mm   ", range_mm);
                i2c_lcd1602_move_cursor(lcdInfo, 0, 1);
                i2c_lcd1602_write_string(lcdInfo, display_buffer);
                ESP_LOGI(TAG, "Range: %d mm", range_mm);
            }
            last_lcd_update = xTaskGetTickCount();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Cleanup
    vl53l0x_stopContinuous(sensorHand);
    vl53l0x_end(sensorHand);
    gpio_set_level(LED_PIN, 0);
}
