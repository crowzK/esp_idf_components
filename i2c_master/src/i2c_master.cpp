#include "i2c_master.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "I2cMaster";

I2cMaster::I2cMaster(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz)
    : _port(port), _is_initialized(false)
{
    _conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = freq_hz}};
}

I2cMaster::~I2cMaster()
{
    if (_is_initialized)
    {
        i2c_driver_delete(_port);
    }
}

esp_err_t I2cMaster::init()
{
    esp_err_t err = i2c_param_config(_port, &_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(_port, _conf.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    _is_initialized = true;
    ESP_LOGI(TAG, "I2C master driver initialized successfully on port %d.", _port);
    return ESP_OK;
}

esp_err_t I2cMaster::write(uint8_t device_address, const uint8_t *write_buffer, size_t write_size, int ticks_to_wait)
{
    if (!_is_initialized)
        return ESP_FAIL;
    return i2c_master_write_to_device(_port, device_address, write_buffer, write_size, pdMS_TO_TICKS(ticks_to_wait));
}

esp_err_t I2cMaster::read(uint8_t device_address, uint8_t *read_buffer, size_t read_size, int ticks_to_wait)
{
    if (!_is_initialized)
        return ESP_FAIL;
    return i2c_master_read_from_device(_port, device_address, read_buffer, read_size, pdMS_TO_TICKS(ticks_to_wait));
}
