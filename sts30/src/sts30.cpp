#include "sts30.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "STS30";

// STS30/SHT3x Commands are compatible for basic operations
static const uint8_t STS30_CMD_MEAS_HIGH_REP[] = {0x24, 0x00}; // Measurement, high repeatability
static const uint8_t STS30_CMD_RESET[] = {0x30, 0xA2};

Sts30::Sts30(i2c_port_t port, uint8_t address) : _i2c_port(port), _address(address) {}

esp_err_t Sts30::begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = freq_hz}};

    esp_err_t err = i2c_param_config(_i2c_port, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(_i2c_port, conf.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    // Send reset command
    i2c_master_write_to_device(_i2c_port, _address, STS30_CMD_RESET, sizeof(STS30_CMD_RESET), pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(50)); // Wait for reset to complete

    ESP_LOGI(TAG, "STS30 initialized successfully on I2C port %d with address 0x%X", _i2c_port, _address);
    return ESP_OK;
}

esp_err_t Sts30::read_temperature(float &temperature)
{
    uint8_t data[3]; // Temperature is 3 bytes: MSB, LSB, CRC

    // Send measurement command
    esp_err_t err = i2c_master_write_to_device(_i2c_port, _address, STS30_CMD_MEAS_HIGH_REP, sizeof(STS30_CMD_MEAS_HIGH_REP), pdMS_TO_TICKS(1000));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Write measurement command failed: %s", esp_err_to_name(err));
        return err;
    }

    // Wait for the measurement to complete (max 15.5ms for high repeatability)
    vTaskDelay(pdMS_TO_TICKS(20));

    // Read 3 bytes of data (Temp MSB, Temp LSB, Temp CRC)
    err = i2c_master_read_from_device(_i2c_port, _address, data, sizeof(data), pdMS_TO_TICKS(1000));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Read sensor data failed: %s", esp_err_to_name(err));
        return err;
    }

    // Check CRC for temperature data
    if (crc8(data, 2) != data[2])
    {
        ESP_LOGE(TAG, "Temperature CRC check failed!");
        return ESP_FAIL;
    }

    // Convert raw data to temperature
    uint16_t raw_temp = (data[0] << 8) | data[1];
    temperature = -45.0f + 175.0f * (float)raw_temp / 65535.0f;

    return ESP_OK;
}

uint8_t Sts30::crc8(const uint8_t *data, int len)
{
    const uint8_t polynomial = 0x31;
    uint8_t crc = 0xFF;

    for (int j = len; j; --j)
    {
        crc ^= *data++;
        for (int i = 8; i; --i)
        {
            crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
        }
    }
    return crc;
}
