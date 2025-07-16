#pragma once

#include "driver/i2c.h"

/**
 * @class Sts30
 * @brief Driver for the Sensirion STS30 high-accuracy temperature sensor.
 */
class Sts30 {
public:
    /**
     * @brief Constructor for the STS30 sensor driver.
     * @param port The I2C port number to use.
     * @param address The I2C address of the sensor (e.g., 0x4A or 0x4B).
     */
    Sts30(i2c_port_t port = I2C_NUM_0, uint8_t address = 0x4A);

    /**
     * @brief Initializes the I2C communication for the sensor.
     * @param sda_pin GPIO number for I2C SDA signal.
     * @param scl_pin GPIO number for I2C SCL signal.
     * @param freq_hz I2C clock frequency.
     * @return esp_err_t ESP_OK on success, otherwise an error code.
     */
    esp_err_t begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz = 100000);

    /**
     * @brief Reads the temperature from the sensor.
     * @param temperature Reference to a float to store the temperature in Celsius.
     * @return esp_err_t ESP_OK on success, otherwise an error code.
     */
    esp_err_t read_temperature(float &temperature);

private:
    i2c_port_t _i2c_port;
    uint8_t _address;

    /**
     * @brief Calculates the CRC-8 checksum for the data.
     * @param data Pointer to the data buffer.
     * @param len Length of the data buffer.
     * @return uint8_t The calculated CRC checksum.
     */
    uint8_t crc8(const uint8_t *data, int len);
};
