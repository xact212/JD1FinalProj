// vl53l0x_platform_esp32.c
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define COMM_SPEED 100000
#define BASE_ADDR 0x29 //0x52 left shifted 1 = 0x29

static const char *TAG = "VL53L0X";
static i2c_master_dev_handle_t sensor_handle = NULL;

// Initialize the I2C device (call this before using the sensor)
VL53L0X_Error VL53L0X_platform_init(i2c_master_bus_handle_t bus_handle, uint8_t address) {
    i2c_device_config_t dev_cfg = {
        .device_address = BASE_ADDR,  // Use 0x29
        .scl_speed_hz = COMM_SPEED,
        .dev_addr_length = 7,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &sensor_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        return -1;
    }
    return 0;
}

// Write multiple bytes (REQUIRED by API)
VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *pdata, uint32_t count) {
    // First byte is the register address (low byte since index is 16-bit)
    uint8_t *buffer = malloc(count + 1);
    if (!buffer) return -1;
    
    buffer[0] = index & 0xFF;  // Register address (VL53L0X uses 8-bit registers)
    memcpy(&buffer[1], pdata, count);
    
    esp_err_t ret = i2c_master_transmit(sensor_handle, buffer, count + 1, 100);
    free(buffer);
    
    return (ret == ESP_OK) ? 0 : -1;
}

// Read multiple bytes (REQUIRED by API)
VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *pdata, uint32_t count) {
    uint8_t reg = index & 0xFF;
    
    esp_err_t ret = i2c_master_transmit_receive(sensor_handle, &reg, 1, pdata, count, 100);
    return (ret == ESP_OK) ? 0 : -1;
}

// Write a single byte (can be a wrapper)
VL53L0X_Error VL53L0X_WriteByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data) {
    return VL53L0X_WriteMulti(Dev, index, &data, 1);
}

// Read a single byte (can be a wrapper)
VL53L0X_Error VL53L0X_ReadByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data) {
    return VL53L0X_ReadMulti(Dev, index, data, 1);
}

// Write a 16-bit word (split into two bytes)
VL53L0X_Error VL53L0X_WriteWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data) {
    uint8_t buffer[2] = { (data >> 8) & 0xFF, data & 0xFF };
    return VL53L0X_WriteMulti(Dev, index, buffer, 2);
}

// Read a 16-bit word (combine two bytes)
VL53L0X_Error VL53L0X_ReadWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data) {
    uint8_t buffer[2];
    VL53L0X_Error status = VL53L0X_ReadMulti(Dev, index, buffer, 2);
    if (status == 0) {
        *data = (buffer[0] << 8) | buffer[1];
    }
    return status;
}

// Write a 32-bit dword (four bytes)
VL53L0X_Error VL53L0X_WriteDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data) {
    uint8_t buffer[4] = {
        (data >> 24) & 0xFF,
        (data >> 16) & 0xFF,
        (data >> 8) & 0xFF,
        data & 0xFF
    };
    return VL53L0X_WriteMulti(Dev, index, buffer, 4);
}

// Read a 32-bit dword (combine four bytes)
VL53L0X_Error VL53L0X_ReadDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data) {
    uint8_t buffer[4];
    VL53L0X_Error status = VL53L0X_ReadMulti(Dev, index, buffer, 4);
    if (status == 0) {
        *data = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    }
    return status;
}

// Wait/delay (REQUIRED by API)
VL53L0X_Error VL53L0X_WaitMs(VL53L0X_DEV Dev, int32_t wait_ms) {
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    return 0;
}

// These can be stubs or simple implementations:
VL53L0X_Error VL53L0X_WaitUs(VL53L0X_DEV Dev, int32_t wait_us) {
    esp_rom_delay_us(wait_us);
    return 0;
}

VL53L0X_Error VL53L0X_WaitValueMaskEx(
    VL53L0X_DEV Dev,
    VL53L0X_Error timeout_ms,
    uint8_t index,
    uint8_t value,
    uint8_t mask,
    VL53L0X_Error poll_delay_ms) {
    
    int32_t elapsed = 0;
    uint8_t byte_value;
    
    while (elapsed < timeout_ms) {
        if (VL53L0X_ReadByte(Dev, index, &byte_value) != 0) {
            return -1;
        }
        if ((byte_value & mask) == value) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(poll_delay_ms));
        elapsed += poll_delay_ms;
    }
    return -1;  // Timeout
}
