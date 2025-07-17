#pragma once

#include "driver/i2c.h"

/**
 * @class I2cMaster
 * @brief A C++ wrapper for the ESP-IDF I2C master driver.
 */
class I2cMaster {
public:
    /**
     * @brief Constructor for I2cMaster.
     * @param port The I2C port number.
     * @param sda_pin GPIO number for I2C SDA signal.
     * @param scl_pin GPIO number for I2C SCL signal.
     * @param freq_hz I2C clock frequency.
     */
    I2cMaster(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz);

    /**
     * @brief Destructor that uninstalls the I2C driver.
     */
    ~I2cMaster();

    /**
     * @brief Initializes the I2C master driver.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t init();

    /**
     * @brief Writes data to an I2C device.
     * @param device_address The 7-bit I2C address of the device.
     * @param write_buffer Pointer to the data to write.
     * @param write_size Size of the data to write.
     * @param ticks_to_wait Ticks to wait for the operation to complete.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t write(uint8_t device_address, const uint8_t* write_buffer, size_t write_size, int ticks_to_wait = 1000);

    /**
     * @brief Reads data from an I2C device.
     * @param device_address The 7-bit I2C address of the device.
     * @param read_buffer Pointer to the buffer to store the read data.
     * @param read_size Size of the data to read.
     * @param ticks_to_wait Ticks to wait for the operation to complete.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t read(uint8_t device_address, uint8_t* read_buffer, size_t read_size, int ticks_to_wait = 1000);

private:
    i2c_port_t _port;
    i2c_config_t _conf;
    bool _is_initialized;
};
