#pragma once

#include "i2c_master.hpp"

/**
 * @class Ina3221
 * @brief Driver for the TI INA3221 Triple-Channel, High-Side Current and Bus Voltage Monitor.
 */
class Ina3221 {
public:
    /**
     * @enum Ina3221Channel
     * @brief Defines the channels available on the INA3221.
     */
    enum Ina3221Channel {
        CHANNEL_1 = 1,
        CHANNEL_2 = 2,
        CHANNEL_3 = 3
    };

    /**
     * @brief Constructor for the INA3221 driver.
     * @param i2c_master A reference to the I2cMaster object for communication.
     * @param address The I2C address of the sensor (default is 0x40).
     */
    Ina3221(I2cMaster& i2c_master, uint8_t address = 0x40);

    /**
     * @brief Initializes the sensor and verifies the connection.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t begin();

    /**
     * @brief Reads the bus voltage for a specific channel.
     * @param channel The channel to read from (CHANNEL_1, CHANNEL_2, or CHANNEL_3).
     * @param voltage Reference to a float to store the bus voltage in Volts.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t get_bus_voltage_V(Ina3221Channel channel, float& voltage);

    /**
     * @brief Reads the shunt voltage for a specific channel.
     * @param channel The channel to read from.
     * @param voltage Reference to a float to store the shunt voltage in millivolts (mV).
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t get_shunt_voltage_mV(Ina3221Channel channel, float& voltage);

    /**
     * @brief Calculates and reads the current for a specific channel.
     * @param channel The channel to read from.
     * @param shunt_resistance_ohm The value of the shunt resistor in Ohms.
     * @param current Reference to a float to store the current in milliamps (mA).
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t get_current_mA(Ina3221Channel channel, float shunt_resistance_ohm, float& current);

    /**
     * @brief Reads the manufacturer ID from the sensor.
     * @param id Reference to a uint16_t to store the ID. Should be 0x5449 for TI.
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t get_manufacturer_id(uint16_t& id);

private:
    I2cMaster& _i2c_master;
    uint8_t _address;

    esp_err_t write_register(uint8_t reg, uint16_t value);
    esp_err_t read_register(uint8_t reg, int16_t& value);
};
