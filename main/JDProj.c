#include <stdio.h>
#include <stdbool.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "vl53l0x.h"
#include "i2c-lcd1602.h"
static const char* TAG = "JDProj";

#define I2C_MASTER_SCL 22 
#define I2C_MASTER_SDA 21
#define I2C_MASTER_PORT I2C_NUM_0 
#define I2C_MASTER_FREQ 100000 
#define I2C_MASTER_TX_BUF 0 
#define I2C_MASTER_RX_BUF 0
#define I2C_MASTER_TIMEOUT_MS 1000

#define VL53L0X_ADDR 0x29 
#define VL530LX_RANGE_MODE VL53L0X_DEVICEMODE_CONTINUOUS_RANGING
#define SCL_INTRPT_POL VL53L0X_INTERRUPTPOLARITY_HIGH
#define SDA_INTRPT_POL VL53L0X_INTERRUPTPOLARITY_HIGH
#define SCL_GPIO_FUNCTIONALITY VL53L0X_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH
#define SDA_GPIO_FUNCTIONALITY VL53L0X_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH

void i2c_master_init(i2c_master_bus_handle_t* bus_handle, 
  i2c_master_dev_handle_t* dev_handle) {
  i2c_master_bus_config_t bus_config = {
    .i2c_port = I2C_MASTER_PORT,
    .sda_io_num = I2C_MASTER_SDA,
    .scl_io_num = I2C_MASTER_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  }; 
  
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

  i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = VL53L0X_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ,
  };

  ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void app_main(void)
{
  //setup I2C bus
  /*
  i2c_master_bus_handle_t bus_handle;
  i2c_master_dev_handle_t dev_handle;
  i2c_master_init(&bus_handle, &dev_handle);
  ESP_LOGI(TAG, "I2C bus initialized successfully");
  */
  //sensor configuration
  vl53l0x_t* sensorHand = vl53l0x_config(
      I2C_MASTER_PORT,
      I2C_MASTER_SCL,
      I2C_MASTER_SDA,
      -1,
      VL53L0X_ADDR,
      1
  );
  if (!sensorHand) {
    ESP_LOGE(TAG, "Failed configuring sensor");
    vl53l0x_end(sensorHand);
    return;
  }
  ESP_LOGI(TAG, "Sensor configured successfully");

  //sensor initialization
  const char* errStr = vl53l0x_init(sensorHand);
  if (errStr != NULL) {
    ESP_LOGE(TAG, "sensor initialization failed: %s", errStr);
    vl53l0x_end(sensorHand);
    return;
  }


  errStr = vl53l0x_setMeasurementTimingBudget(sensorHand, 50000);  // 50ms
  if (errStr) {
      ESP_LOGW(TAG, "Failed to set timing budget: %s", errStr);
  }

  //lcd initialization
  ESP_LOGI(TAG, "\n=== Continuous Mode ===");
  ESP_LOGI(TAG, "Starting continuous ranging (200ms interval)...");
  
  vl53l0x_startContinuous(sensorHand, 200);  // 200ms between measurements
  
  while (true) {
      uint16_t range_mm = vl53l0x_readRangeContinuousMillimeters(sensorHand);
      
      if (vl53l0x_timeoutOccurred(sensorHand)) {
          ESP_LOGW(TAG, "Measurement timeout!");
      } else {
          ESP_LOGI(TAG, "Range: %d mm", range_mm);
      }
      
      // Check for I2C errors
      if (vl53l0x_i2cFail(sensorHand)) {
          ESP_LOGE(TAG, "I2C communication error!");
          break;
      }
      
      vTaskDelay(pdMS_TO_TICKS(250));  // Slightly longer than measurement interval
  }
  
  vl53l0x_stopContinuous(sensorHand);
  ESP_LOGI(TAG, "Continuous ranging stopped");
  vl53l0x_end(sensorHand);
  return;
  //main loop
  //poll sensor
  //update lcd
  //hosuekeeping
}
