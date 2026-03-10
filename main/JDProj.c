#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char* TAG = "JDProj";

#define I2C_MASTER_SCL 39 
#define I2C_MASTER_SDA 42
#define I2C_MASTER_PORT I2C_NUM_0 
#define I2C_MASTER_FREQ 100000 
#define I2C_MASTER_TX_BUF 0 
#define I2C_MASTER_RX_BUF 0
#define I2C_MASTER_TIMEOUT_MS 1000

#define VL530X_ADDR 0x52 

static void i2c_master_init(i2c_master_bus_handle_t* bus_handle, i2c_master_dev_handle_t* dev_handle) {
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
    .device_address = VL530X_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ,
  };

  ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void app_main(void)
{
  i2c_master_bus_handle_t bus_handle;
  i2c_master_dev_handle_t dev_handle;
  i2c_master_init(&bus_handle, &dev_handle);
  ESP_LOGI(TAG, "I2C initialized successfully");
}
