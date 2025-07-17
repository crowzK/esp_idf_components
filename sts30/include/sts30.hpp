#pragma once

#include "i2c_master.hpp" // Use the new I2cMaster class

/**
 * @class Sts30
 * @brief Driver for the Sensirion STS30 high-accuracy temperature sensor.
 */
class Sts30 {
public:
    /**
     * @brief Constructor for the STS30 sensor driver.
     * @param i2c_master A reference to the I2cMaster object for communication.
     * @param address The I2C address of the sensor (e.g., 0x4A or 0x4B).
     */
    Sts30(I2cMaster& i2c_master, uint8_t address = 0x4A);

    /**
     * @brief Checks if the sensor is connected and responsive.
     * @return esp_err_t ESP_OK on success, otherwise an error code.
     */
    esp_err_t begin();

    /**
     * @brief Reads the temperature from the sensor.
     * @param temperature Reference to a float to store the temperature in Celsius.
     * @return esp_err_t ESP_OK on success, otherwise an error code.
     */
    esp_err_t read_temperature(float &temperature);

private:
    I2cMaster& _i2c_master; // Store a reference to the I2C master
    uint8_t _address;

    /**
     * @brief Calculates the CRC-8 checksum for the data.
     * @param data Pointer to the data buffer.
     * @param len Length of the data buffer.
     * @return uint8_t The calculated CRC checksum.
     */
    uint8_t crc8(const uint8_t *data, int len);
};
